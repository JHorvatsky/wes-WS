SET(SOURCES screens/ui_Home_Scr.c
    screens/ui_Color_Scr.c
    screens/ui_glazbascr.c
    screens/ui_InfoScr.c
    screens/ui_ppgscr.c
    screens/ui_photoScr.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    ui_events.c
    images/ui_img_bl_logo_symbol_png.c
    images/ui_img_home_icon_png.c)

add_library(ui ${SOURCES})
