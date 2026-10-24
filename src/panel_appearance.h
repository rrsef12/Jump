// Semantic colors for compact-panel actions. Selection remains blue; scheme
// controls are violet; active automation uses a muted teal icon on neutral
// surfaces, and limitations use amber.
struct PanelAppearance {
    COLORREF fill, edge, ink, iconInk;
    UiIcon icon;
    bool dropdown=false;
};
std::optional<PanelAppearance> panelAppearance(int id,bool open,bool enabled,int hover,bool pressed){
    PanelAppearance style{g->surface,RGB(53,63,78),g->fg,RGB(167,183,205),UiIcon::Info};
    const COLORREF activeIcon=RGB(148,177,170);
    auto active=[&]{style.fill=g->surface;style.edge=RGB(62,73,83);
        style.ink=g->fg;style.iconInk=activeIcon;};
    auto limited=[&]{style.fill=RGB(48,39,28);style.edge=RGB(91,72,44);
        style.ink=RGB(235,199,140);style.iconInk=RGB(224,172,94);};
    switch(id){
    case ID_STRUCT_NAME:
        style={RGB(43,35,60),RGB(83,66,111),RGB(218,201,246),RGB(190,157,239),UiIcon::Layers,true};break;
    case ID_STRUCT_SCHEMES:
        style.icon=UiIcon::Settings;style.dropdown=true;
        if(!g->parts.prefix.empty()||!g->parts.suffix.empty()||g->ruleEnabled)style.iconInk=activeIcon;
        break;
    case ID_NUMBER_MENU:
        style.icon=UiIcon::Number;style.dropdown=true;
        if(!g->structure.free&&g->numberMode){active();if(g->dialog.directory.empty()||!g->ruleError.empty())limited();}
        break;
    case ID_AFFIX_SUMMARY:
        style.icon=UiIcon::Affix;
        if(!g->parts.prefix.empty()||!g->parts.suffix.empty())active();else style.ink=g->muted;
        break;
    case ID_REPLACE_SUMMARY:
        style.icon=UiIcon::Replace;
        if(g->ruleEnabled&&!g->rulePattern.empty())active();else style.ink=g->muted;
        break;
    case ID_RULES_SUMMARY:
        style.icon=UiIcon::Settings;style.ink=g->muted;style.dropdown=true;break;
    case ID_COMBO_UNDO:style.icon=UiIcon::Undo;break;
    case ID_COMBO_REDO:style.icon=UiIcon::Redo;break;
    default:return std::nullopt;
    }
    if(!enabled){style.fill=g->bg;style.edge=RGB(39,45,55);style.ink=style.iconInk=RGB(96,106,122);}
    else{
        if(open)style.edge=style.iconInk;
        if(hover)style.fill=mixColor(style.fill,RGB(255,255,255),hover*6/100);
        if(pressed)style.fill=mixColor(style.fill,RGB(0,0,0),12);
    }
    return style;
}
