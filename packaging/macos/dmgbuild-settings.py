import os

# 'defines' is injected by dmgbuild when called with -D key=value.
application = defines.get('app', 'PLACEHOLDER')
appname = os.path.basename(application)
license_file = defines.get('license', '')

format = 'UDZO'
files = [application] + ([license_file] if license_file else [])
symlinks = {'Applications': '/Applications'}

background = defines.get('background', 'background.png')

show_status_bar = False
show_tab_view   = False
show_toolbar    = False
show_pathbar    = False
show_sidebar    = False
sidebar_width   = 0

window_rect    = ((980, 526), (600, 388))
default_view   = 'icon-view'
show_icon_preview = False
icon_size      = 64
text_size      = 16
grid_offset    = (0, 0)
grid_spacing   = 100
scroll_position = (0, 0)
label_pos      = 'bottom'
arrange_by     = None

icon_locations = {
    appname:        (150, 125),
    'Applications': (300, 125),
    'license.rtf':  (450, 125),
}