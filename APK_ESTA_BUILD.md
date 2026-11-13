# APK de esta compilación (todo integrado)

**Ejecución:** [Build APK #43344401036](https://github.com/luan920530-commits/Hurricane-Android/actions/runs/43344401036)
**Rama:** `arena/cd11f369-hurricane-android` · **commit:** `6724a3f` (fusión de `main`)
**Artefacto:** `hurrican-android-debug` — **45,2 MB** (antes 43,7 MB: +1,5 MB de arte nuevo)

## Descargar

1. <https://github.com/luan920530-commits/Hurricane-Android/actions/runs/43344401036>
2. Abajo, en **Artifacts** → **hurrican-android-debug**
3. Descomprime el `.zip` → `app-debug.apk`

```bash
adb install -r app-debug.apk
```

> La primera vez tras esta versión **se vuelven a copiar los datos** (~10 s con cartel de
> carga): los iconos táctiles y la skin son ficheros nuevos y se ha subido la versión de
> assets.

## Qué hay ahora en el APK

De esta rama (lo que ya probaste):
- Escalado **Nítido / Suave / Máximo** (panel DBG → *Calidad de imagen*).
- **Pausa automática** al salir de la app con una partida en marcha.
- Todas las correcciones de pantalla negra / cierres.

De `main` (nuevo aquí):
- **Mando con arte**: marco táctico metálico + iconos dibujados (antes eran círculos).
- **El layout se guarda**: lo que coloques en modo edición se queda entre ejecuciones.
- **Acciones directas**: el mando responde aunque el perfil guardado sea "joystick".
- **Tienda offline** en el menú (1 diamante = 1 moneda; vidas, arma, bombas).
- **Personaje femenino** seleccionable.
- Trucos nuevos: **maná infinito** y **super salto** (además de los que ya había).

## Qué mirar

| Zona | Qué probar |
|---|---|
| Arranque | cartel de carga + copia de datos; no debe cerrarse |
| Mando | ¿el marco y los iconos se ven bien y no tapan demasiado? |
| Guardado del layout | mover un botón en modo edición → cerrar la app → abrir: sigue ahí |
| Calidad de imagen | comparar las tres en el panel y decirme cuál prefieres |
| Tienda | entrar, comprar una vida, empezar partida nueva y ver si se aplica |
| Personaje | seleccionar el femenino y comprobar las animaciones al correr/saltar |
| Pausa | salir a inicio en mitad de partida y volver |

## Si algo falla

```bash
adb logcat -s Hurrican
adb shell run-as org.hurrican.game cat files/save/Game_Log.txt
```

Los ficheros que guarda la app ahora son: `save/Hurrican.cfg`, `save/touch-layout-v1.txt`
(posición de los botones), `save/offline-store-v1.txt` (monedas y compras) y
`save/Game_Log.txt`.
