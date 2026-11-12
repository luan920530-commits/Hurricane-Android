# APK de esta compilación

**Ejecución:** [Build APK #43219213957](https://github.com/luan920530-commits/Hurricane-Android/actions/runs/43219213957)
**Rama:** `arena/cd11f369-hurricane-android` · **commit:** `cbfd5a8`
**Artefacto:** `hurrican-android-debug` (45,84 MB), válido hasta el **12/12/2026**.

## Descargar

1. Abre la ejecución: <https://github.com/luan920530-commits/Hurricane-Android/actions/runs/43219213957>
2. Abajo, en **Artifacts**, pulsa **hurrican-android-debug**. Se descarga un `.zip`.
3. Descomprime: dentro está `app-debug.apk` (y los símbolos de depuración).

> El ZIP hay que descomprimirlo: la APK de dentro es la que se instala. Si el móvil avisa de
> "aplicación de origen desconocido", hay que permitir la instalación desde esa app concreta.

## Instalar

```bash
adb install -r app-debug.apk
```

O copiando la APK al móvil y abriéndola. Es **debug**: convive sin problema con una versión
anterior, pero hay que desinstalar la antigua si cambió la firma.

## Qué mirar en esta build

| Zona | Qué probar |
|---|---|
| Arranque | Debe verse el cartel de carga y, en el primer arranque, la copia de datos (~91 MB, ~10 s). Ya no debe cerrarse en negro |
| Mando | Joystick flotante a la izquierda (aparece donde toques), botones en arco a la derecha: **B** dispara, **A** salta |
| Panel DBG | Botón arriba a la derecha: vidas/munición infinitas, invulnerable, FPS, editar controles y **Calidad de imagen** |
| Calidad de imagen | Compara *Nítido* / *Suave* / *Máximo* en el panel: se aplica al instante. Dime cuál se ve mejor |
| Pausa | Sal de la app (botón inicio) con una partida en marcha y vuelve: debe estar en el menú con *Continuar partida* y no disparar solo |
| Contexto GL | Minimiza y vuelve varias veces: no debe quedarse en negro |

## Si algo falla

```bash
adb logcat -s Hurrican                    # registro del motor en vivo
adb shell run-as org.hurrican.game cat files/save/Game_Log.txt
```

Pásame las últimas ~30 líneas: casi todo lo que puede fallar deja ahí el motivo (shader,
textura, fichero que falta). El motivo de un cierre también se añade al final de ese fichero.
