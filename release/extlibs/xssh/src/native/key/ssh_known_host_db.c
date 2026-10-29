#include <xrt/ssh_known_host_db.h>

#include "ssh_key_text_internal.h"
#include "ssh_known_host_internal.h"

#include <string.h>



#if defined(XSSH_FEATURE_KNOWN_HOST_DB)

#define XSSH_KNOWN_HOST_DB_VALID_FLAGS \
	((uint32)XSSH_KNOWN_HOST_DB_STRICT)



/* 判断去掉行结束符后的文本是否为空行或注释。 */
static bool xsshKnownHostDbIgnorable(xstrview Line)
{
	size_t i = 0u;

	while ( (i < Line.Size) && xsshKeyTextSpace(
		(unsigned char)Line.Data[i]
	) ) {
		++i;
	}
	return (i == Line.Size) || (Line.Data[i] == '#');
}



/* 校验游标和输出不会在推进时覆盖借用数据库。 */
static bool xsshKnownHostDbStateValid(
	const xsshknownhostdb* pDatabase,
	const xsshknownhostentry* pEntry
)
{
	return xrtMemRangeValid(pDatabase, sizeof(*pDatabase)) &&
		xrtMemRangeValid(pEntry, sizeof(*pEntry)) &&
		xrtMemRangeValid(pDatabase->Source.Data, pDatabase->Source.Size) &&
		(pDatabase->Position <= pDatabase->Source.Size) &&
		((pDatabase->Flags & ~XSSH_KNOWN_HOST_DB_VALID_FLAGS) == 0u) &&
		!xrtMemRangesOverlap(
			pDatabase,
			sizeof(*pDatabase),
			pEntry,
			sizeof(*pEntry)
		) && !xrtMemRangesOverlap(
			pDatabase->Source.Data,
			pDatabase->Source.Size,
			pDatabase,
			sizeof(*pDatabase)
		) && !xrtMemRangesOverlap(
			pDatabase->Source.Data,
			pDatabase->Source.Size,
			pEntry,
			sizeof(*pEntry)
		);
}



/* 返回一行无结束符借用视图，并在局部游标中推进位置和行号。 */
static xsshcode xsshKnownHostDbLine(
	xsshknownhostdb* pDatabase,
	xstrview* pLine
)
{
	size_t iStart = pDatabase->Position;
	size_t iEnd = iStart;

	if ( iStart == pDatabase->Source.Size ) {
		return XSSH_NEED_MORE;
	}
	if ( pDatabase->LineNumber == SIZE_MAX ) {
		return XSSH_ERROR_OVERFLOW;
	}
	while ( (iEnd < pDatabase->Source.Size) &&
		(pDatabase->Source.Data[iEnd] != '\n') ) {
		++iEnd;
	}
	pDatabase->Position = iEnd + (iEnd < pDatabase->Source.Size ? 1u : 0u);
	++pDatabase->LineNumber;
	if ( (iEnd > iStart) &&
		(pDatabase->Source.Data[iEnd - 1u] == '\r') ) {
		--iEnd;
	}
	pLine->Data = pDatabase->Source.Data + iStart;
	pLine->Size = iEnd - iStart;
	return XSSH_OK;
}



/* 匹配明文或 OpenSSH |1| 主机字段，并统一否定项为不匹配。 */
static xsshcode xsshKnownHostDbHostMatch(
	const xsshknownhostline* pKnownHost,
	xstrview Host,
	uint32 iPort,
	bool* pMatch
)
{
	if ( pKnownHost->Hashed ) {
		return xrtSshKnownHostLineHashMatch(
			pKnownHost,
			Host,
			iPort,
			pMatch
		);
	} else {
		xsshknownhostmatch Match;
		xsshcode Code = xrtSshKnownHostLineMatch(
			pKnownHost,
			Host,
			iPort,
			&Match
		);

		if ( Code != XSSH_OK ) {
			return Code;
		}
		*pMatch = Match == XSSH_KNOWN_HOST_MATCH;
		return XSSH_OK;
	}
}



/* 生成没有决定性来源行的 NEW 结果。 */
static void xsshKnownHostDbNew(xsshknownhostcheck* pCheck)
{
	memset(pCheck, 0, sizeof(*pCheck));
	pCheck->Trust = XSSH_KNOWN_HOST_TRUST_NEW;
}



/* 初始化只借用文本的数据库游标。 */
xsshcode xrtSshKnownHostDbInit(
	xsshknownhostdb* pDatabase,
	xstrview Source,
	uint32 iFlags
)
{
	xsshknownhostdb Database;

	if ( !xrtMemRangeValid(pDatabase, sizeof(*pDatabase)) ||
		!xrtMemRangeValid(Source.Data, Source.Size) ||
		((iFlags & ~XSSH_KNOWN_HOST_DB_VALID_FLAGS) != 0u) ||
		xrtMemRangesOverlap(
			Source.Data,
			Source.Size,
			pDatabase,
			sizeof(*pDatabase)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Database.Source = Source;
	Database.Position = 0u;
	Database.LineNumber = 0u;
	Database.Flags = iFlags;
	*pDatabase = Database;
	return XSSH_OK;
}



/* 逐行解析借用文本，严格模式把第一个坏行作为显式条目返回。 */
xsshcode xrtSshKnownHostDbNext(
	xsshknownhostdb* pDatabase,
	xsshknownhostentry* pEntry
)
{
	xsshknownhostdb Database;
	xsshknownhostentry Entry;
	xstrview Line;
	xsshcode Code;

	if ( !xsshKnownHostDbStateValid(pDatabase, pEntry) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Database = *pDatabase;
	for ( ;; ) {
		Code = xsshKnownHostDbLine(&Database, &Line);
		if ( Code != XSSH_OK ) {
			if ( Code == XSSH_NEED_MORE ) {
				*pDatabase = Database;
			}
			return Code;
		}
		if ( xsshKnownHostDbIgnorable(Line) ) {
			continue;
		}
		memset(&Entry, 0, sizeof(Entry));
		Entry.Source = Line;
		Entry.LineNumber = Database.LineNumber;
		Code = xrtSshKnownHostLineRead(Line, &Entry.KnownHost);
		if ( Code == XSSH_OK ) {
			Entry.Valid = true;
			break;
		}
		if ( (Code != XSSH_ERROR_PROTOCOL) &&
			(Code != XSSH_ERROR_UNSUPPORTED) ) {
			return Code;
		}
		if ( (Database.Flags & (uint32)XSSH_KNOWN_HOST_DB_STRICT) != 0u ) {
			break;
		}
	}
	*pDatabase = Database;
	*pEntry = Entry;
	return XSSH_OK;
}



/* 扫描完整文本并按安全优先级汇总普通 key、撤销和 CA 记录。 */
xsshcode xrtSshKnownHostDbCheck(
	xstrview Source,
	xstrview Host,
	uint32 iPort,
	xbytesview KeyBlob,
	uint32 iFlags,
	xsshknownhostcheck* pCheck
)
{
	xsshknownhosttarget Target;
	xsshpublickey PublicKey;
	xsshknownhostdb Database;
	xsshknownhostentry Entry;
	xsshknownhostentry MatchEntry;
	xsshknownhostentry ChangedEntry;
	xsshknownhostentry CaEntry;
	xsshknownhostcheck Check;
	bool bHaveMatch = false;
	bool bHaveChanged = false;
	bool bHaveCa = false;
	xsshcode Code;

	if ( !xrtMemRangeValid(pCheck, sizeof(*pCheck)) ||
		!xrtMemRangeValid(Source.Data, Source.Size) ||
		!xrtMemRangeValid(KeyBlob.Data, KeyBlob.Size) ||
		!xsshKnownHostTargetInit(&Target, Host, iPort) ||
		xrtMemRangesOverlap(
			Source.Data,
			Source.Size,
			pCheck,
			sizeof(*pCheck)
		) || xrtMemRangesOverlap(
			Host.Data,
			Host.Size,
			pCheck,
			sizeof(*pCheck)
		) || xrtMemRangesOverlap(
			KeyBlob.Data,
			KeyBlob.Size,
			pCheck,
			sizeof(*pCheck)
		) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	(void)Target;
	Code = xrtSshPublicKeyRead(KeyBlob, &PublicKey);
	if ( Code == XSSH_NEED_MORE ) {
		return XSSH_ERROR_PROTOCOL;
	}
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshKnownHostDbInit(&Database, Source, iFlags);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	for ( ;; ) {
		bool bHostMatch;
		bool bKeyMatch;

		Code = xrtSshKnownHostDbNext(&Database, &Entry);
		if ( Code == XSSH_NEED_MORE ) {
			break;
		}
		if ( Code != XSSH_OK ) {
			return Code;
		}
		if ( !Entry.Valid ||
			(Entry.KnownHost.MarkerKind == XSSH_KNOWN_HOST_MARKER_UNKNOWN) ) {
			if ( (iFlags & (uint32)XSSH_KNOWN_HOST_DB_STRICT) != 0u ) {
				Check.Trust = XSSH_KNOWN_HOST_TRUST_INVALID;
				Check.Entry = Entry;
				*pCheck = Check;
				return XSSH_OK;
			}
			continue;
		}
		Code = xsshKnownHostDbHostMatch(
			&Entry.KnownHost,
			Host,
			iPort,
			&bHostMatch
		);
		if ( Code != XSSH_OK ) {
			if ( ((iFlags & (uint32)XSSH_KNOWN_HOST_DB_STRICT) != 0u) &&
				(Code == XSSH_ERROR_PROTOCOL) ) {
				Entry.Valid = false;
				memset(&Entry.KnownHost, 0, sizeof(Entry.KnownHost));
				Check.Trust = XSSH_KNOWN_HOST_TRUST_INVALID;
				Check.Entry = Entry;
				*pCheck = Check;
				return XSSH_OK;
			}
			if ( Code == XSSH_ERROR_PROTOCOL ) {
				continue;
			}
			return Code;
		}
		if ( !bHostMatch ) {
			continue;
		}
		if ( Entry.KnownHost.MarkerKind ==
			XSSH_KNOWN_HOST_MARKER_CERT_AUTHORITY ) {
			if ( !bHaveCa ) {
				CaEntry = Entry;
				bHaveCa = true;
			}
			continue;
		}
		Code = xrtSshKnownHostLineKeyMatch(
			&Entry.KnownHost,
			KeyBlob,
			&bKeyMatch
		);
		if ( Code != XSSH_OK ) {
			return Code;
		}
		if ( Entry.KnownHost.MarkerKind ==
			XSSH_KNOWN_HOST_MARKER_REVOKED ) {
			if ( bKeyMatch ) {
				Check.Trust = XSSH_KNOWN_HOST_TRUST_REVOKED;
				Check.Entry = Entry;
				*pCheck = Check;
				return XSSH_OK;
			}
			continue;
		}
		if ( bKeyMatch ) {
			if ( !bHaveMatch ) {
				MatchEntry = Entry;
				bHaveMatch = true;
			}
		} else if ( !bHaveChanged && xsshKeyTextEqual(
			Entry.KnownHost.Algorithm,
			PublicKey.Algorithm
		) ) {
			ChangedEntry = Entry;
			bHaveChanged = true;
		}
	}
	if ( bHaveMatch ) {
		Check.Trust = XSSH_KNOWN_HOST_TRUST_MATCH;
		Check.Entry = MatchEntry;
	} else if ( bHaveCa ) {
		Check.Trust = XSSH_KNOWN_HOST_TRUST_CERT_AUTHORITY;
		Check.Entry = CaEntry;
	} else if ( bHaveChanged ) {
		Check.Trust = XSSH_KNOWN_HOST_TRUST_CHANGED;
		Check.Entry = ChangedEntry;
	} else {
		xsshKnownHostDbNew(&Check);
	}
	*pCheck = Check;
	return XSSH_OK;
}

#endif
