/**
 * Dialogo de error para Android.
 *
 * Cuando el motor no puede arrancar (faltan datos, no se pudo crear el contexto OpenGL,
 * etc.) el jugador solo veia una pantalla negra y la app se cerraba sin explicacion. Este
 * modulo muestra el motivo con un dialogo del sistema, que ademas es lo que se necesita
 * para que la gente pueda reportar el problema.
 *
 * (c) 2026 - Port de Android, misma licencia que el resto del codigo (MIT).
 **/

#ifndef _ANDROIDERROR_HPP_
#define _ANDROIDERROR_HPP_

#if defined(ANDROID)

#  include <string>

/**
 * Muestra un dialogo modal con el mensaje de error.
 *
 * Debe llamarse una vez SDL esta inicializado (lo esta en GameInit, que es donde se detectan
 * casi todos los fallos). No vuelve hasta que el usuario pulsa Aceptar.
 *
 * @param title   titulo del dialogo
 * @param message detalle del problema
 * @param log_tail ultimas lineas del log a mostrar (util para copiar el error a mano)
 */
void Android_ShowFatalError(const std::string &title, const std::string &message,
                            const std::string &log_tail = std::string());

#endif /* ANDROID */

#endif /* _ANDROIDERROR_HPP_ */
