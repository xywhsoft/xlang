#include "src/driver/xl6_cli.h"
#include "lib/xrt/xrt.h"

#ifdef _WIN32
#include <shellapi.h>
#endif

int main(int argc, char** argv)
{
    int rc;

    if ( xrtInit() == NULL ) {
        return 1;
    }

#ifdef _WIN32
    {
        int wideArgc;
        wchar_t** wideArgv;

        wideArgc = 0;
        wideArgv = CommandLineToArgvW(GetCommandLineW(), &wideArgc);
        if ( wideArgv != NULL && wideArgc > 0 ) {
            char** utf8Argv;
            int i;

            utf8Argv = (char**)xrtCalloc((size_t)wideArgc + 1, sizeof(char*));
            if ( utf8Argv != NULL ) {
                for ( i = 0; i < wideArgc; ++i ) {
                    if ( wideArgv[i] != NULL && wideArgv[i][0] != L'\0' ) {
                        utf8Argv[i] = (char*)xrtUTF16to8((u16str)wideArgv[i], 0, NULL);
                    }
                    if ( utf8Argv[i] == NULL ) {
                        utf8Argv[i] = "";
                    }
                }
                rc = xl6CliMain(wideArgc, utf8Argv);
                for ( i = 0; i < wideArgc; ++i ) {
                    if ( wideArgv[i] != NULL && wideArgv[i][0] != L'\0' && utf8Argv[i][0] != '\0' ) {
                        xrtFree(utf8Argv[i]);
                    }
                }
                xrtFree(utf8Argv);
                LocalFree(wideArgv);
                xrtUnit();
                return rc;
            }
        }
        if ( wideArgv != NULL ) {
            LocalFree(wideArgv);
        }
    }
#endif

    rc = xl6CliMain(argc, argv);
    xrtUnit();
    return rc;
}
