#ifndef XSSH_H
#define XSSH_H

#include <xssh/features.h>
#include <xrt.h>
#include <xrt/ssh_wire.h>
#include <xrt/ssh_packet.h>
#include <xrt/ssh_packet_random.h>
#include <xrt/ssh_packet_aes_gcm.h>
#include <xrt/ssh_packet_codec.h>
#include <xrt/ssh_packet_codec_random.h>
#include <xrt/ssh_kexinit.h>
#include <xrt/ssh_kexinit_random.h>
#include <xrt/ssh_kex_ecdh.h>
#include <xrt/ssh_kex_sha256.h>
#include <xrt/ssh_kex_curve25519.h>
#include <xrt/ssh_kex_curve25519_random.h>
#include <xrt/ssh_hostkey.h>
#include <xrt/ssh_hostkey_ed25519.h>
#include <xrt/ssh_key_text.h>
#include <xrt/ssh_known_host.h>
#include <xrt/ssh_known_host_hash.h>
#include <xrt/ssh_known_host_db.h>
#include <xrt/ssh_fingerprint.h>
#include <xrt/ssh_private_key.h>
#include <xrt/ssh_private_key_pem.h>
#include <xrt/ssh_private_key_ed25519.h>
#include <xrt/ssh_transport_message.h>
#include <xrt/ssh_transport_rekey.h>
#include <xrt/ssh_transport_state.h>
#include <xrt/ssh_transport_core.h>
#include <xrt/ssh_kex_session.h>
#include <xrt/ssh_kex_session_random.h>
#include <xrt/ssh_kex_exchange.h>
#include <xrt/ssh_kex_exchange_random.h>
#include <xrt/ssh_transport_tcp.h>
#include <xrt/ssh_transport_tcp_random.h>
#include <xrt/ssh_auth_message.h>
#include <xrt/ssh_auth_password.h>
#include <xrt/ssh_auth_publickey.h>
#include <xrt/ssh_auth_keyboard.h>
#include <xrt/ssh_auth_hostbased.h>
#include <xrt/ssh_auth_guard.h>
#include <xrt/ssh_auth_session.h>
#include <xrt/ssh_connection_message.h>
#include <xrt/ssh_channel_message.h>
#include <xrt/ssh_channel_window.h>
#include <xrt/ssh_channel_request.h>
#include <xrt/ssh_channel_pty.h>
#include <xrt/ssh_channel_state.h>
#include <xrt/ssh_channel_core.h>
#include <xrt/ssh_channel_io.h>
#include <xrt/ssh_forward_message.h>
#include <xrt/ssh_reply_queue.h>
#include <xrt/ssh_connection_session.h>
#include <xrt/ssh_channels.h>
#include <xrt/ssh_session_core.h>
#include <xrt/ssh_session_core_random.h>
#include <xrt/ssh_session_tcp.h>
#include <xrt/ssh_session_reader.h>
#include <xrt/ssh_session_stream.h>
#include <xrt/ssh_session_tcp_random.h>
#include <xrt/ssh_client_core.h>
#include <xrt/ssh_client_auth_ed25519.h>
#include <xrt/ssh_client.h>
#include <xrt/ssh_client_dial.h>
#include <xrt/ssh_client_future.h>
#include <xrt/ssh_client_session.h>
#include <xrt/ssh_client_forward.h>
#include <xrt/ssh_client_pty.h>

#if defined(XSSH_FEATURE_SSH) && \
	(!defined(XSSH_FEATURE_SESSION_STREAM) || \
	 !defined(XSSH_FEATURE_CLIENT_CORE) || \
	 !defined(XSSH_FEATURE_CLIENT_AUTH_ED25519) || \
	 !defined(XSSH_FEATURE_CLIENT) || \
	 !defined(XSSH_FEATURE_CLIENT_DIAL) || \
	 !defined(XSSH_FEATURE_CLIENT_FUTURE) || \
	 !defined(XSSH_FEATURE_CLIENT_SESSION) || \
	 !defined(XSSH_FEATURE_CLIENT_FORWARD) || \
	 !defined(XSSH_FEATURE_CLIENT_PTY))
	#error "XSSH_FEATURE_SSH requires stream and client session support"
#endif

#endif
