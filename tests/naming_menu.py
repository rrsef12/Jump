"""Native menu navigation for Jump's owned integration fixtures."""
import ctypes as c
from ctypes import wintypes as w
import time
import native_integration as n

u=n.u
u.GetMenuItemCount.argtypes=[c.c_void_p]
u.GetMenuStringW.argtypes=[c.c_void_p,c.c_uint,c.c_wchar_p,c.c_int,c.c_uint]
u.GetMenuState.argtypes=[c.c_void_p,c.c_uint,c.c_uint]
u.GetSubMenu.argtypes=[c.c_void_p,c.c_int];u.GetSubMenu.restype=c.c_void_p
u.GetMenuItemID.argtypes=[c.c_void_p,c.c_int];u.GetMenuItemID.restype=c.c_uint
u.GetMenuItemRect.argtypes=[w.HWND,c.c_void_p,c.c_uint,c.POINTER(w.RECT)]

def labels(menu):
    result=[]
    for i in range(u.GetMenuItemCount(menu)):
        buffer=c.create_unicode_buffer(256)
        u.GetMenuStringW(menu,i,buffer,len(buffer),0x400)
        result.append(buffer.value)
    return result

def find(menu,path):
    path=[path] if isinstance(path,str) else path
    for depth,wanted in enumerate(path):
        captions=labels(menu)
        index=next((i for i,value in enumerate(captions) if value==wanted or value.startswith(wanted+' · ')),None)
        assert index is not None,(wanted,captions)
        if depth+1<len(path):
            menu=u.GetSubMenu(menu,index);assert menu,(wanted,'has no submenu')
    return menu,index

def actions(menu):
    result=[]
    for i,caption in enumerate(labels(menu)):
        child=u.GetSubMenu(menu,i)
        if child:result+=actions(child)
        elif caption:result.append(u.GetMenuItemID(menu,i))
    return result

def select_item(popup,menu,index,activate=True):
    # Native keyboard navigation targets only this owned popup. Mouse hover
    # and submenu animation must not change which command a check selects.
    def highlighted():
        return next((i for i in range(u.GetMenuItemCount(menu))
                     if u.GetMenuState(menu,i,0x400)&0x80),None)
    for _ in range(u.GetMenuItemCount(menu)+1):
        previous=highlighted()
        if previous==index:break
        u.PostMessageW(popup,0x100,0x28,1)
        n.wait(lambda:highlighted()!=previous,label='owned menu item '+str(index))
    assert highlighted()==index,(index,'not highlighted')
    if not activate:return None
    child=u.GetSubMenu(menu,index)
    u.PostMessageW(popup,0x100,0x27 if child else 0x0D,1)
    return child

def select(panel,pid,path=None,exclude=(),disabled=(),checked=(),capture=None,required=(),anchor=790,next_menu=False,dark=False,max_width=None,tip_expected=None):
    n.wait(lambda:n.panel_ready(panel));n.activate(panel)
    # A no-op live edit can still have a delayed fill queued after panel_ready.
    time.sleep(.2);n.wait(lambda:n.panel_ready(panel))
    old=w.POINT();u.GetCursorPos(c.byref(old))
    rect=w.RECT();u.GetWindowRect(panel,c.byref(rect));u.SetCursorPos(rect.left+4,rect.top+4)
    assert u.GetDlgItem(panel,anchor), 'the menu entry must be visible'
    u.PostMessageW(panel,0x111,anchor,u.GetDlgItem(panel,anchor))
    popup=n.wait(lambda:next(iter(n.windows(pid,'#32768',True)),None),label='naming settings menu')
    time.sleep(.25)
    menu=u.SendMessageW(popup,0x1E1,0,0)
    root=labels(menu)
    if dark:
        from fill_paint_integration import pixel
        color=pixel(popup)
        assert all((color>>shift)&255<120 for shift in [0,8,16]),('menu background is not dark',color)
    if max_width:
        rect=w.RECT();u.GetWindowRect(popup,c.byref(rect))
        assert rect.right-rect.left<=max_width,('menu exceeds width limit',rect.right-rect.left,max_width)
    if tip_expected:
        owner,index=find(menu,tip_expected);select_item(popup,owner,index,activate=False)
        tip=n.wait(lambda:next((h for h in n.windows(pid,'tooltips_class32',True) if n.text(h)==tip_expected),None),label='complete long menu name tooltip')
        if capture:
            import extended_integration as e
            e.capture(tip,capture.with_name(capture.stem+'-full-name.png'))
    if capture:
        import extended_integration as e
        e.capture(popup,capture)
    assert not any(value in root for value in exclude),root
    for value in disabled:
        owner,index=find(menu,value);assert u.GetMenuState(owner,index,0x400)&3,(value,root)
    for value in checked:
        owner,index=find(menu,value);assert u.GetMenuState(owner,index,0x400)&8,(value,root)
    if required:
        ids=actions(menu)
        assert len(ids)==len(set(ids)),('duplicate command entry',ids)
        assert set(required)<=set(ids),('missing command',set(required)-set(ids))
    if path is None:
        u.PostMessageW(panel,0x1F,0,0)
        n.wait(lambda:not n.windows(pid,'#32768',True))
        u.SetCursorPos(old.x,old.y)
        return root
    path=[path] if isinstance(path,str) else path
    try:
        for depth,wanted in enumerate(path):
            captions=labels(menu)
            index=next((i for i,value in enumerate(captions) if value==wanted or value.startswith(wanted+' · ')),None)
            assert index is not None,(wanted,captions)
            assert not u.GetMenuState(menu,index,0x400)&3,(wanted,'disabled')
            child=select_item(popup,menu,index)
            if depth+1<len(path):
                assert child,(wanted,'has no submenu')
                popup=n.wait(lambda:next((h for h in n.windows(pid,'#32768',True)
                    if u.SendMessageW(h,0x1E1,0,0)==child),None),label='submenu '+wanted)
                menu=child;time.sleep(.25)
        if not next_menu:n.wait(lambda:not n.windows(pid,'#32768',True))
    finally:u.SetCursorPos(old.x,old.y)
    return root
