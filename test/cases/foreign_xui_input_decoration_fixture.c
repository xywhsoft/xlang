#include "test/cases/foreign_xui_input_decoration.h"

int fixtureAddInputDecoration(xui_widget input, int side, float width)
{
    xui_input_decoration_desc_t decoration = {0};
    decoration.iSize = sizeof(decoration);
    decoration.iKind = XUI_INPUT_DECORATION_TEXT;
    decoration.sText = "observed";
    decoration.iVisibleMode = XUI_INPUT_DECORATION_VISIBLE_ALWAYS;
    decoration.fWidth = width;
    return xuiInputDecorationAdd(input, side, NULL, &decoration);
}
