#include "src/runtime/xl6_process_internal.h"

/* Explicit terminal configuration roots the native PTY/ConPTY component.
 * Ordinary commands remain independent of the optional terminal backend. */
void xl6ProcessCommandTerminalV1(void* handle,bool enabled)
{
    XL6_ProcessCommand* command=xl6ProcessCommandCheck(handle);if(!command)return;
    if(enabled && (command->stdio[0].owned || command->stdio[1].owned || command->stdio[2].owned)){
        xrtSetErrorKind(XERR_UNSUPPORTED);return;
    }
    command->config.Terminal=enabled;
    if(enabled){command->config.HideWindow=false;command->config.NewConsole=false;}
}
void xl6ProcessCommandTerminalSizeV1(void* handle,int64_t columns,int64_t rows)
{
    XL6_ProcessCommand* command=xl6ProcessCommandCheck(handle);if(!command)return;
    /* Both native backends use the signed 16-bit terminal dimension range. */
    if(columns<=0 || rows<=0 || columns>32767 || rows>32767){xrtSetErrorKind(XERR_RANGE);return;}
    command->config.Columns=(uint32)columns;command->config.Rows=(uint32)rows;
}
