project = "ns-3 Continuum"
copyright = "2026, UCC"
author = "John Mullan"

extensions = [
    "breathe",
    "sphinx_rtd_dark_mode",
]

breathe_projects = {
    "distributed": "_build/doxygen/xml",
}

breathe_default_project = "distributed"

exclude_patterns = ["_build"]

html_theme = "sphinx_rtd_theme"

html_theme_options = {
    "navigation_depth": 3,
}

html_favicon = 'static/favicon.svg'

default_dark_mode = True

html_use_index = False
