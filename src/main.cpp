#include <Geode/Geode.hpp>
#include "../include/IMEExtension.hpp"

using namespace geode::prelude;
using namespace ime;

class IMEExtensionDispatcher::Impl {
public:
    std::unordered_set<IMEExtensionDelegate*> m_delegates;

    Impl() = default;
    ~Impl() = default;

    void notifyCandidateList(std::vector<std::u32string> const& candidates, size_t currentCandidate) {
        for (auto delegate : m_delegates) {
            delegate->candidateList(candidates, currentCandidate);
        }
    }

    void notifyComposition(std::u32string const& compositionString) {
        for (auto delegate : m_delegates) {
            delegate->composition(compositionString);
        }
    }
};

class ime::IMEExtensionDispatcherImpl {
public:
    static IMEExtensionDispatcher::Impl* get() {
        return IMEExtensionDispatcher::get()->m_impl.get();
    }
};

IMEExtensionDispatcher* IMEExtensionDispatcher::get() {
    static IMEExtensionDispatcher instance;
    return &instance;
}

IMEExtensionDispatcher::IMEExtensionDispatcher() : m_impl(std::make_unique<Impl>()) {}
IMEExtensionDispatcher::~IMEExtensionDispatcher() = default;

void IMEExtensionDispatcher::addDelegate(IMEExtensionDelegate* delegate) {
    m_impl->m_delegates.insert(delegate);
}
void IMEExtensionDispatcher::removeDelegate(IMEExtensionDelegate* delegate) {
    m_impl->m_delegates.erase(delegate);
}

IMEExtensionDelegate::IMEExtensionDelegate() {
    IMEExtensionDispatcher::get()->addDelegate(this);
}

IMEExtensionDelegate::~IMEExtensionDelegate() {
    IMEExtensionDispatcher::get()->removeDelegate(this);
}

#ifdef GEODE_IS_WINDOWS

#include <Geode/cocos/robtop/glfw/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#include <Geode/cocos/robtop/glfw/glfw3native.h>

LONG_PTR g_origWindowProc;

void updateCandidateList(HIMC himc, HWND hwnd) {
    DWORD candidateSize = ImmGetCandidateListW(himc, 0, nullptr, 0);
    // log::debug("IME Candidate List Size: {}", candidateSize);
    if (candidateSize > 0) {
        std::vector<std::byte> buffer(candidateSize);

        auto candidateList = reinterpret_cast<CANDIDATELIST*>(buffer.data());
        ImmGetCandidateListW(himc, 0, candidateList, candidateSize);

        std::vector<std::u32string> candidates;
        size_t currentCandidate = candidateList->dwSelection;

        for (UINT i = 0; i < candidateList->dwCount; ++i) {
            LPCWSTR str = reinterpret_cast<LPCWSTR>(buffer.data() + candidateList->dwOffset[i]);
            // log::debug("IME Candidate {}: {}", i, string::wideToUtf8(str));
            auto string32 = string::utf8ToUtf32(string::wideToUtf8(str)).unwrapOrDefault();
            candidates.push_back(string32);
        }

        IMEExtensionDispatcherImpl::get()->notifyCandidateList(candidates, currentCandidate);
    }
}

LRESULT CALLBACK HookedWindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_CREATE:
        {
            HIMC himc = ImmGetContext(hwnd);
            ImmAssociateContext(hwnd, himc);

            // Set composition window position
            COMPOSITIONFORM cf = {};
            cf.dwStyle = CFS_POINT;
            ImmSetCompositionWindow(himc, &cf);

            ImmReleaseContext(hwnd, himc);

            DWORD classStyle = GetClassLongPtr(hwnd, GCL_STYLE);
            // log::debug("IME enabled: {}", (classStyle & CS_IME) != 0);
            return 0;
        }
        case WM_IME_STARTCOMPOSITION: {
            return 0;
        }
        case WM_IME_ENDCOMPOSITION: {
            return 0;
        }
        case WM_IME_COMPOSITION:
        {
            HIMC himc = ImmGetContext(hwnd);
            if (lparam & GCS_RESULTSTR) {
                DWORD dwSize = ImmGetCompositionStringW(himc, GCS_RESULTSTR, nullptr, 0);
                if (dwSize > 0) {
                    std::wstring result(dwSize / sizeof(wchar_t), 0);
                    ImmGetCompositionStringW(himc, GCS_RESULTSTR, &result[0], dwSize);
                    auto utf8str = string::wideToUtf8(result);
                    CCIMEDispatcher::sharedDispatcher()->dispatchInsertText(utf8str.c_str(), utf8str.size(), enumKeyCodes::KEY_Unknown);
                    // log::debug("IME Composition Result: {}", (char*)result.c_str());
                }
            }
            else if (lparam & GCS_COMPSTR) {
                DWORD dwSize = ImmGetCompositionStringW(himc, GCS_COMPSTR, nullptr, 0);
                if (dwSize > 0) {
                    std::wstring compStr(dwSize / sizeof(wchar_t), 0);
                    ImmGetCompositionStringW(himc, GCS_COMPSTR, &compStr[0], dwSize);
                    auto string32 = string::utf8ToUtf32(string::wideToUtf8(compStr)).unwrapOrDefault();
                    IMEExtensionDispatcherImpl::get()->notifyComposition(string32);
                    // log::debug("IME Composition String: {}", (char*)compStr.c_str());
                }
            }
            else if (lparam == 0) {
                IMEExtensionDispatcherImpl::get()->notifyComposition(U"");
            }
            ImmReleaseContext(hwnd, himc);
            return 0;
        }
        case WM_IME_NOTIFY:
        {
            // log::debug("IME Notify: {}", wparam);
            switch (wparam) {
                case IMN_SETCANDIDATEPOS:
                case IMN_CHANGECANDIDATE:
                case IMN_CLOSECANDIDATE:
                case IMN_OPENCANDIDATE:
                    HIMC himc = ImmGetContext(hwnd);
                    updateCandidateList(himc, hwnd);
                    ImmReleaseContext(hwnd, himc);
                    return 0;
            }
        }
    }
windoworig:
    return CallWindowProc((WNDPROC)g_origWindowProc, hwnd, msg, wparam, lparam);
}

void modifyWindowProc() {
    g_origWindowProc = SetWindowLongPtrA(WindowFromDC(wglGetCurrentDC()), -4, (LONG_PTR)HookedWindowProc);
}

#include <Geode/modify/CCEGLView.hpp>
class $modify(MyEGLView, CCEGLView){
    static auto onModify(auto& self) {
        (void)self.setHookPriority("cocos2d::CCEGLView::onGLFWKeyCallback", Priority::Stub);
    }

    static MyEGLView* get() {
        return static_cast<MyEGLView*>(CCEGLView::get());
    }

    void performSafeClipboardPaste() {
        auto clipboard = clipboard::read();
        if (clipboard.size() > 0) {
            CCIMEDispatcher::sharedDispatcher()->dispatchInsertText(clipboard.c_str(), static_cast<int>(clipboard.size()), KEY_Unknown);
        }
    }

	// void onGLFWKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    //     auto dispatcher = CCIMEDispatcher::sharedDispatcher();
    //     // log::debug("Key pressed: {} (scancode: {}, action: {}, mods: {})", key, scancode, action, mods);
    //     if ((key == 0x101 || key <= 0x109 && key >= 0x106) && dispatcher->hasDelegate()) {
    //         std::string text = "a";
    //         enumKeyCodes keyCode = enumKeyCodes::KEY_Unknown;
    //         switch (key) {
    //             case 0x101:
    //                 text = "\n";
    //                 break;
    //             case 0x106:
    //                 keyCode = enumKeyCodes::KEY_Right;
    //                 break;
    //             case 0x107:
    //                 keyCode = enumKeyCodes::KEY_Left;
    //                 break;
    //             case 0x108:
    //                 keyCode = enumKeyCodes::KEY_Down;
    //                 break;
    //             case 0x109:
    //                 keyCode = enumKeyCodes::KEY_Up;
    //                 break;
    //         }
    //         if (action >= 1) {
    //             dispatcher->dispatchInsertText(text.c_str(), text.size(), keyCode);
    //         }
    //         return;
    //     }
    //     if (action >= 1 && key == 'V' && (mods & GLFW_MOD_CONTROL)) {
    //         auto clipboard = clipboard::read();
    //         if (clipboard.size() > 0) {
    //             dispatcher->dispatchInsertText(clipboard.c_str(), static_cast<int>(clipboard.size()), KEY_Unknown);
    //         }
    //         return;
    //     }
	// 	CCEGLView::onGLFWKeyCallback(window, key, scancode, action, mods);
	// }

	void onGLFWCharCallback(GLFWwindow* window, unsigned int codepoint) {
        // log::debug("Character input: {}", codepoint);
        if (codepoint < 0x80) {
            CCEGLView::onGLFWCharCallback(window, codepoint);
            return;
        }
        auto const u32char = static_cast<char32_t>(codepoint);
        auto const u32view = std::u32string_view(&u32char, 1);
        if (GEODE_UNWRAP_IF_OK(utf8Char, string::utf32ToUtf8(u32view))) {
            CCIMEDispatcher::sharedDispatcher()->dispatchInsertText(utf8Char.c_str(), utf8Char.size(), enumKeyCodes::KEY_Unknown);
        }
    }
};

$on_mod(Loaded) {
    // This function is called when the mod is loaded
    // log::info("Mod loaded successfully!");

    Loader::get()->queueInMainThread([](){
        modifyWindowProc();
    });
}

#endif