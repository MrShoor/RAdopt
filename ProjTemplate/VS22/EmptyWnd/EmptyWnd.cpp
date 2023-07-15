#include <RWnd.h>

class MyWnd : public RA::UIRenderWindow {
private:
public:
    MyWnd() : RA::UIRenderWindow(L"MyWnd", true, true) {};
};


int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    MyWnd wnd;
    RA::MessageLoop([&wnd]() {
            WaitForSingleObject(INVALID_HANDLE_VALUE, 1);
        });
    return 0;
}
