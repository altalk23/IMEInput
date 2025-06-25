#pragma once

#include <Geode/DefaultInclude.hpp>
#include <Geode/utils/cocos.hpp>
#include <cocos2d.h>
#include <unordered_map>
#include <unordered_set>

#ifdef GEODE_IS_WINDOWS
    #ifdef IME_INPUT_EXPORTING
        #define IME_INPUT_DLL __declspec(dllexport)
    #else
        #define IME_INPUT_DLL __declspec(dllimport)
    #endif
#else
    #define IME_INPUT_DLL __attribute__((visibility("default")))
#endif

namespace ime {
    class IME_INPUT_DLL IMEExtensionDelegate {
    public:
        IMEExtensionDelegate();
        virtual ~IMEExtensionDelegate();
        
        virtual void candidateList(std::vector<std::u32string> const& candidates, size_t currentCandidate) = 0;
        virtual void composition(std::u32string const& compositionString) = 0;
    };

    class IMEExtensionDispatcherImpl;

    class IME_INPUT_DLL IMEExtensionDispatcher {
        class Impl;
        std::unique_ptr<Impl> m_impl;
    public:
        static IMEExtensionDispatcher* get();
        IMEExtensionDispatcher();
        ~IMEExtensionDispatcher();

        void addDelegate(IMEExtensionDelegate* delegate);
        void removeDelegate(IMEExtensionDelegate* delegate);

        friend class IMEExtensionDispatcherImpl;
    };
}