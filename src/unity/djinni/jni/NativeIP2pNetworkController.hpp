// AUTOGENERATED FILE - DO NOT MODIFY!
// This file generated by Djinni from libunity.djinni

#pragma once

#include "djinni_support.hpp"
#include "i_p2p_network_controller.hpp"

namespace djinni_generated {

class NativeIP2pNetworkController final : ::djinni::JniInterface<::IP2pNetworkController, NativeIP2pNetworkController> {
public:
    using CppType = std::shared_ptr<::IP2pNetworkController>;
    using CppOptType = std::shared_ptr<::IP2pNetworkController>;
    using JniType = jobject;

    using Boxed = NativeIP2pNetworkController;

    ~NativeIP2pNetworkController();

    static CppType toCpp(JNIEnv* jniEnv, JniType j) { return ::djinni::JniClass<NativeIP2pNetworkController>::get()._fromJava(jniEnv, j); }
    static ::djinni::LocalRef<JniType> fromCppOpt(JNIEnv* jniEnv, const CppOptType& c) { return {jniEnv, ::djinni::JniClass<NativeIP2pNetworkController>::get()._toJava(jniEnv, c)}; }
    static ::djinni::LocalRef<JniType> fromCpp(JNIEnv* jniEnv, const CppType& c) { return fromCppOpt(jniEnv, c); }

private:
    NativeIP2pNetworkController();
    friend ::djinni::JniClass<NativeIP2pNetworkController>;
    friend ::djinni::JniInterface<::IP2pNetworkController, NativeIP2pNetworkController>;

};

}  // namespace djinni_generated
