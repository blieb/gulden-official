// AUTOGENERATED FILE - DO NOT MODIFY!
// This file generated by Djinni from libunity.djinni

#pragma once

#include "djinni_support.hpp"
#include "i_witness_controller.hpp"

namespace djinni_generated {

class NativeIWitnessController final : ::djinni::JniInterface<::IWitnessController, NativeIWitnessController> {
public:
    using CppType = std::shared_ptr<::IWitnessController>;
    using CppOptType = std::shared_ptr<::IWitnessController>;
    using JniType = jobject;

    using Boxed = NativeIWitnessController;

    ~NativeIWitnessController();

    static CppType toCpp(JNIEnv* jniEnv, JniType j) { return ::djinni::JniClass<NativeIWitnessController>::get()._fromJava(jniEnv, j); }
    static ::djinni::LocalRef<JniType> fromCppOpt(JNIEnv* jniEnv, const CppOptType& c) { return {jniEnv, ::djinni::JniClass<NativeIWitnessController>::get()._toJava(jniEnv, c)}; }
    static ::djinni::LocalRef<JniType> fromCpp(JNIEnv* jniEnv, const CppType& c) { return fromCppOpt(jniEnv, c); }

private:
    NativeIWitnessController();
    friend ::djinni::JniClass<NativeIWitnessController>;
    friend ::djinni::JniInterface<::IWitnessController, NativeIWitnessController>;

};

}  // namespace djinni_generated
