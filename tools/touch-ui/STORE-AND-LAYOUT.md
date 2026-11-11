# Tienda y disposición táctil de Android

La tienda offline se abre desde el menú principal. Por cada diamante de una partida
normal se añade una moneda (sin sustituir sus efectos originales); tutorial y demos
no conceden monedas. Se pueden comprar vidas, mejoras permanentes para el arma
principal y bombas inteligentes. Los precios, límites y reglas visibles para el
jugador están en `Hurrican/data/TIENDA-Y-CONTROLES.txt`.

El saldo y las compras se guardan en `offline-store-v1.txt` en el directorio de
configuración de la aplicación; las vidas y bombas se entregan al jugador 1 al
comenzar una partida nueva y se consumen, y las mejoras de arma son permanentes
para partidas nuevas. No se modifican partidas cargadas ni tutoriales. Skins es
un marcador sin compra. Los datos son locales, sin pagos ni sincronización; al
borrar datos o desinstalar pueden perderse. El archivo inválido se rechaza, no se
sobrescribe. Las escrituras usan archivo temporal y renombrado; no son un sistema
antifraude ni garantizan supervivencia ante todo fallo de energía.

El editor actual permite arrastrar controles, no cambiarles el tamaño; su
configuración se guarda en `touch-layout-v1.txt`, se valida antes de aplicarla y
se conserva fuera de los assets. El botón DBG abre el panel y no se arrastra en
ese modo. Los tests unitarios nativos `test-store.cpp` y `test-layout.cpp` se
compilan en el workflow Android; son complementarios a probar el APK en un móvil.
