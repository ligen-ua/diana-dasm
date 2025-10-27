
#include "oui_disasm_colors.h"

namespace oui
{

    static DisasmColorsProfile g_disasmColors = 
    {
        // address
        {
            {
                ColorWhite(),     // text
                ColorBlack(),     // background
            },
            {
                ColorWhite(),     // text
                ColorBlack(),     // background
            }
        },
        // bytes
        {
            {
                ColorGray(),      // text
                ColorBlack(),     // background
            },
            {
                ColorGray(),      // text
                ColorBlack(),     // background
            }
        },
        // command
        {
            {
                ColorWhite(),      // text
                ColorBlack(),      // background
            },
            {
                ColorWhite(),      // text
                ColorBlack(),      // background
            }
        },
        // spaces
        {
            {
                ColorWhite(),      // text
                ColorBlack(),      // background
            },
            {
                ColorWhite(),      // text
                ColorBlack(),      // background
            }
        }
    };
    void QueryDefaultColorProfile(DisasmColorsProfile& profile)
    {
        profile = g_disasmColors;
    }

}