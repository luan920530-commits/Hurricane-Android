/*
 * Escalado del framebuffer interno (640x480) a la pantalla del movil.
 *
 * Es el mismo vertex shader que shader_render.vert, pero separado para que el
 * fragment shader de escalado pueda evolucionar sin arrastrar la simulacion CRT.
 * El juego solo lee los atributos por nombre, asi que este fichero tambien sirve
 * para GLSL 100 (GLES2) y para GLSL 320 (GLES3).
 */
uniform mat4 u_MVPMatrix;
attribute vec2 a_Position;
attribute vec4 a_Color;
attribute vec2 a_Texcoord0;

varying vec4 v_Color;
varying vec2 v_Texcoord0;

void main()
{
    v_Color = a_Color;
    v_Texcoord0 = a_Texcoord0;
    gl_Position = u_MVPMatrix * vec4(a_Position, 0.0, 1.0);
}
