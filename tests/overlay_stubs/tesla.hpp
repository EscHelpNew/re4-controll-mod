#pragma once
#include <switch.h>
#include <algorithm>
#include <cstdio>
#include <functional>
#include <memory>
#include <new>
#include <cstdlib>
#include <string>
#include <stdexcept>
#include <utility>
#include <vector>
#ifdef OVERLAY_TEST_TRACK_ALLOCATIONS
namespace overlay_test_allocations {
inline bool track=false;
inline std::size_t largest=0;
}
void* operator new(std::size_t size){
    if(overlay_test_allocations::track)overlay_test_allocations::largest=std::max(overlay_test_allocations::largest,size);
    if(void* memory=std::malloc(size?size:1))return memory;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size){return ::operator new(size);}
void operator delete(void* memory)noexcept{std::free(memory);}
void operator delete[](void* memory)noexcept{std::free(memory);}
void operator delete(void* memory,std::size_t)noexcept{std::free(memory);}
void operator delete[](void* memory,std::size_t)noexcept{std::free(memory);}
#endif
namespace tsl {
namespace cfg {constexpr unsigned FramebufferWidth=448,FramebufferHeight=720;}
namespace style::color {constexpr unsigned ColorDescription=1,ColorText=2;}
namespace gfx {
struct Renderer {
    struct Text {std::string value;int x,y;};
    struct Rect {int x,y,w,h;unsigned color;};
    struct Bitmap {int x,y,w,h;const void* source;unsigned first,distinct;};
    std::vector<Text> text;
    std::vector<Rect> rects;
    std::vector<Bitmap> bitmaps;
    static unsigned a(unsigned c){return c;}
    void drawRect(int x,int y,int w,int h,unsigned color){rects.push_back({x,y,w,h,color});}
    void drawBitmap(int x,int y,int w,int h,const u8* pixels){
        unsigned first=pixels[0]|unsigned(pixels[1])<<8|unsigned(pixels[2])<<16,distinct=0;
        for(int i=0;i<w*h;++i) {
            const auto rgb=pixels[i*4]|unsigned(pixels[i*4+1])<<8|unsigned(pixels[i*4+2])<<16;
            if(rgb!=first)++distinct;
            if(pixels[i*4+3]!=255)throw std::runtime_error("nonopaque game illustration");
        }
        bitmaps.push_back({x,y,w,h,pixels,first,distinct});
    }
    std::pair<unsigned,unsigned> drawString(const char* s,bool,int x,int y,float,unsigned,int=0) {
        text.push_back({s,x,y});return {0,0};
    }
};
}
namespace elm {
class Element {
protected:
    constexpr static inline auto a=&gfx::Renderer::a;
public:
    unsigned x=0,y=0,width=0,height=0;
    virtual ~Element()=default;
    virtual void draw(gfx::Renderer*){}
    virtual void layout(u16,u16,u16,u16){}
    virtual void setFocused(bool){}
    void setBoundaries(unsigned a,unsigned b,unsigned c,unsigned d){x=a;y=b;width=c;height=d;}
    void invalidate(){}
};
class ListItem:public Element {
public:
    std::string label,value;
    unsigned text_sets=0;
    std::function<bool(u64)> click;
    ListItem(std::string a,std::string b=""):label(a),value(b){}
    void setClickListener(std::function<bool(u64)> f){click=f;}
    void setText(std::string s){label=s;++text_sets;}
    void setValue(std::string s){value=s;}
};
class ToggleListItem:public ListItem {
public:
    bool state;std::function<void(bool)> changed;
    ToggleListItem(std::string label,bool value,std::string,std::string):ListItem(label),state(value){}
    void setStateChangedListener(std::function<void(bool)> f){changed=f;}
    void setState(bool value){state=value;}
};
class CategoryHeader:public Element {
public:
    std::string label;
    explicit CategoryHeader(const char* value):label(value){}
    void setText(const std::string& value){label=value;}
};
class List:public Element {
public:
    std::vector<Element*> children;
    ~List(){for(auto* p:children)delete p;}
    void addItem(Element* e){children.push_back(e);}
};
class OverlayFrame:public Element {
protected: Element* m_contentElement=nullptr;
public:
    std::string title,subtitle;
    OverlayFrame(const std::string& a,const std::string& b):title(a),subtitle(b){}
    void setTitle(const std::string& value){title=value;}
    void setSubtitle(const std::string& value){subtitle=value;}
    ~OverlayFrame(){delete m_contentElement;}
    void setContent(Element* p){m_contentElement=p;}
    Element* content(){return m_contentElement;}
    void layout(u16 a,u16 b,u16 c,u16 d)override{setBoundaries(a,b,c,d);}
};
}
class Gui {
public:
    elm::Element* focused=nullptr;
    elm::Element* root=nullptr;
    virtual ~Gui(){delete root;}
    virtual elm::Element* createUI()=0;
    virtual void update(){}
    elm::Element* getFocusedElement(){return focused;}
};
inline std::vector<std::unique_ptr<Gui>> navigation;
template<class T,class... Args>std::unique_ptr<Gui>& changeTo(Args&&... args){
    auto gui=std::make_unique<T>(std::forward<Args>(args)...);
    gui->root=gui->createUI();navigation.push_back(std::move(gui));return navigation.back();
}
inline void goBack(){if(!navigation.empty())navigation.pop_back();}
class Overlay {
public:
    virtual ~Overlay()=default;
    virtual void initServices(){}
    virtual void exitServices(){}
    virtual std::unique_ptr<Gui> loadInitialGui(){return {};}
    template<class T>std::unique_ptr<Gui> initially(){return std::make_unique<T>();}
};
template<class T>int loop(int,char**){return 0;}
}
