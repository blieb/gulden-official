// AUTOGENERATED FILE - DO NOT MODIFY!
// This file generated by Djinni from libunity.djinni

#include "NativeMonitorListener.hpp"  // my header
#include "Marshal.hpp"

namespace djinni_generated {

NativeMonitorListener::NativeMonitorListener() : ::djinni::JniInterface<::MonitorListener, NativeMonitorListener>() {}

NativeMonitorListener::~NativeMonitorListener() = default;

NativeMonitorListener::JavaProxy::JavaProxy(JniType j) : Handle(::djinni::jniGetThreadEnv(), j) { }

NativeMonitorListener::JavaProxy::~JavaProxy() = default;

void NativeMonitorListener::JavaProxy::onPartialChain(int32_t c_height, int32_t c_probable_height, int32_t c_offset) {
    auto jniEnv = ::djinni::jniGetThreadEnv();
    ::djinni::JniLocalScope jscope(jniEnv, 10);
    const auto& data = ::djinni::JniClass<::djinni_generated::NativeMonitorListener>::get();
    jniEnv->CallVoidMethod(Handle::get().get(), data.method_onPartialChain,
                           ::djinni::get(::djinni::I32::fromCpp(jniEnv, c_height)),
                           ::djinni::get(::djinni::I32::fromCpp(jniEnv, c_probable_height)),
                           ::djinni::get(::djinni::I32::fromCpp(jniEnv, c_offset)));
    ::djinni::jniExceptionCheck(jniEnv);
}
void NativeMonitorListener::JavaProxy::onPruned(int32_t c_height) {
    auto jniEnv = ::djinni::jniGetThreadEnv();
    ::djinni::JniLocalScope jscope(jniEnv, 10);
    const auto& data = ::djinni::JniClass<::djinni_generated::NativeMonitorListener>::get();
    jniEnv->CallVoidMethod(Handle::get().get(), data.method_onPruned,
                           ::djinni::get(::djinni::I32::fromCpp(jniEnv, c_height)));
    ::djinni::jniExceptionCheck(jniEnv);
}
void NativeMonitorListener::JavaProxy::onProcessedSPVBlocks(int32_t c_height) {
    auto jniEnv = ::djinni::jniGetThreadEnv();
    ::djinni::JniLocalScope jscope(jniEnv, 10);
    const auto& data = ::djinni::JniClass<::djinni_generated::NativeMonitorListener>::get();
    jniEnv->CallVoidMethod(Handle::get().get(), data.method_onProcessedSPVBlocks,
                           ::djinni::get(::djinni::I32::fromCpp(jniEnv, c_height)));
    ::djinni::jniExceptionCheck(jniEnv);
}

}  // namespace djinni_generated
