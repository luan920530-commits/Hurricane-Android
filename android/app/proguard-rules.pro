# SDL accede a sus clases y metodos por JNI: no deben renombrarse ni eliminarse.
-keep class org.libsdl.app.** { *; }
-keep class org.hurrican.game.** { *; }
-keepclasseswithmembernames class * {
    native <methods>;
}
