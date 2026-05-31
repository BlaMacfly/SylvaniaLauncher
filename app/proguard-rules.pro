# kotlinx.serialization — keep generated serializers.
-keepattributes *Annotation*, InnerClasses
-dontnote kotlinx.serialization.**
-keepclassmembers class com.sylvania.launcher.** {
    *** Companion;
}
-keepclasseswithmembers class com.sylvania.launcher.** {
    kotlinx.serialization.KSerializer serializer(...);
}
