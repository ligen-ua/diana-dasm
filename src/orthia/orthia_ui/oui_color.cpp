#include "oui_color.h"

namespace oui
{
    Color ColorBlack()
    {
        return Color(ColorChannel::None, ColorChannel::None, ColorChannel::None);
    }
    Color ColorBlue()
    {
        return Color(ColorChannel::None, ColorChannel::None, ColorChannel::Low);
    }
    Color ColorGreen()
    {
        return Color(ColorChannel::None, ColorChannel::Low, ColorChannel::None);
    }
    Color ColorCyan()
    {
        return Color(ColorChannel::None, ColorChannel::Low, ColorChannel::Low);
    }
    Color ColorRed()
    {
        return Color(ColorChannel::Low, ColorChannel::None, ColorChannel::None);
    }
    Color ColorMagenta()
    {
        return Color(ColorChannel::Low, ColorChannel::None, ColorChannel::Low);
    }
    Color ColorYellow()
    {
        return Color(ColorChannel::Low, ColorChannel::Low, ColorChannel::None);
    }
    Color ColorWhite()
    {
        return Color(ColorChannel::Normal, ColorChannel::Normal, ColorChannel::Normal);
    }
    Color ColorGray()
    {
        return Color(ColorChannel::Low, ColorChannel::Low, ColorChannel::Low);
    }
    Color ColorBrightBlue()
    {
        return Color(ColorChannel::None, ColorChannel::None, ColorChannel::High);
    }
    Color ColorBrightGreen()
    {
        return Color(ColorChannel::None, ColorChannel::High, ColorChannel::None);
    }
    Color ColorBrightCyan()
    {
        return Color(ColorChannel::None, ColorChannel::High, ColorChannel::High);
    }
    Color ColorBrightRed()
    {
        return Color(ColorChannel::High, ColorChannel::None, ColorChannel::None);
    }
    Color ColorBrightMagenta()
    {
        return Color(ColorChannel::High, ColorChannel::None, ColorChannel::High);
    }
    Color ColorBrightYellow()
    {
        return Color(ColorChannel::High, ColorChannel::High, ColorChannel::None);
    }
    Color ColorBrightWhite()
    {
        return Color(ColorChannel::High, ColorChannel::High, ColorChannel::High);
    }

    // color defaults
    static MenuColorProfile g_menuColorProfile =
    {
        // menu
        {
            // backgroundColor
            ColorBlack(),

            // button normal
            {
                ColorWhite(),     // buttonText
                ColorBlack(),     // buttonBackground
                ColorCyan(),      // buttonHotkeyText
            },
            // highlight
            {
                ColorBlack(),     // buttonHighlightedText
                ColorCyan(),      // buttonHighlightedBackground
                ColorBlack()      // buttonHighlightedHotkeyText
            }
        },
        // popup
        {
            // border colors
            ColorWhite(),
            ColorBlack(),

            // button normal
            {
                ColorWhite(),     // buttonText
                ColorBlack(),     // buttonBackground
                ColorCyan(),      // buttonHotkeyText
            },
            // highlight
            {
                ColorBlack(),     // buttonHighlightedText
                ColorCyan(),      // buttonHighlightedBackground
                ColorBlack()      // buttonHighlightedHotkeyText
            }
        }
    };

    void QueryDefaultColorProfile(MenuColorProfile& profile)
    {
        profile = g_menuColorProfile;
    }

    // PanelColorProfile
    static PanelColorProfile g_panelColorProfile =
    {
        // border
        {
            ColorGray() // borderText
        },
        {
            ColorBlack() // borderBackground
        },
        // button normal
        {
            ColorWhite(),     // buttonText
            ColorBlack(),     // buttonBackground
            ColorCyan(),      // buttonHotkeyText
        },
        // selected
        {
            ColorCyan(),     // buttonHighlightedText
            ColorBlack(),    // buttonHighlightedBackground
            ColorCyan()      // buttonHighlightedHotkeyText
        },
        // highlight
        {
            ColorBlack(),     // buttonHighlightedText
            ColorCyan(),      // buttonHighlightedBackground
            ColorBlack()      // buttonHighlightedHotkeyText
        }
    };

    void QueryDefaultColorProfile(PanelColorProfile& profile)
    {
        profile = g_panelColorProfile;
    }

    // label
    static LabelColorProfile g_labelColorProfile =
    {
        {
            ColorWhite(),     // text
            ColorBlack(),     // background
        },
        {
            ColorWhite(),     // text
            ColorBlack(),     // background
        }
    };
    void QueryDefaultColorProfile(LabelColorProfile& profile)
    {
        profile = g_labelColorProfile;
    }

    // listbox
    static ListBoxColorProfile g_listBoxColorProfile =
    {
        {
            ColorGray(),     // text
            ColorBlack(),     // background
        },
        {
            ColorBrightWhite(),     // text
            ColorCyan(),     // background
        },
        ColorGray()
    };
    static Color g_listBoxFolders = ColorWhite();

    void QueryDefaultColorProfile(ListBoxColorProfile& profile)
    {
        profile = g_listBoxColorProfile;
    }

    // editbox
    static EditBoxColorProfile g_editBoxColorProfile =
    {
        {
            ColorWhite(),     // text
            ColorBlack(),      // background
        },
        {
            ColorBlack(),     // text
            ColorYellow(),     // background
        }
    };

    void QueryDefaultColorProfile(EditBoxColorProfile& profile)
    {
        profile = g_editBoxColorProfile;
    }

    // dialog
    void QueryDefaultColorProfile(DialogColorProfile& profile)
    {
        QueryDefaultColorProfile(profile.label);
        QueryDefaultColorProfile(profile.listBox);
        profile.listBoxFolders = g_listBoxFolders;
        QueryDefaultColorProfile(profile.editBox);

    }

}