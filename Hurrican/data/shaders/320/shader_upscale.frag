/*
 * Escalado del framebuffer interno (640x480) a la pantalla del movil.
 *
 * El juego se disena a 640x480 y un movil tiene entre 720 y 1600 pixeles de alto, asi que
 * este paso decide casi toda la nitidez que ve el jugador. El escalado por defecto (una
 * sola muestra con filtro lineal) deja los bordes de los sprites y de las letras sucios.
 *
 * Tres calidades, elegidas con la constante c_upscale (la pone el motor; se puede cambiar
 * en caliente desde el panel DBG):
 *
 *   0 = nitido  -> vecino mas cercano: pixeles perfectos, como en la consola original
 *   1 = suave   -> escalado de calidad (por defecto)
 *   2 = maximo  -> escalado de calidad + muestras extra (quita el parpadeo al moverse)
 *
 * El suavizado es una version reducida de xBR/ScaleHQ (Hyllian, licencia MIT): si el pixel
 * esta en una zona plana devuelve su color tal cual (no emborrona texturas), y si esta en
 * un borde mezcla solo en la direccion del borde.
 *
 * IMPORTANTE (precision): GLSL ES declara `precision mediump float` por defecto, y mediump
 * tiene unos 10 bits de mantisa. Con coordenadas de 640 px, `fract(uv * 640.0)` sale mal
 * (640.5 no es representable) y el suavizado se vuelve ruido. Por eso los calculos de
 * coordenadas usan highp cuando el hardware lo permite.
 */

in vec4 v_Color;
in vec2 v_Texcoord0;

uniform sampler2D u_Texture0;
uniform int u_Time;

/* Salida: en GLSL 320 es no existe v_FragColor. */
out vec4 v_FragColor;

/*
 * Tamano del framebuffer interno (640x480). CShader inyecta las constantes como enteros
 * literales antes de compilar, igual que hace con las opciones de CRT.
 */
const vec2 TEX_SIZE = vec2(float(c_TexWidth), float(c_TexHeight));

#define HIGHP highp

const float Curvature = 6.0;
const float color_bleeding = 1.1;
const float bleeding_range = 1.3;

vec2 crt_coords(vec2 uv, float bend)
{
    uv -= 0.5;
    uv *= 2.0;
    uv.x *= 1.0 + pow(abs(uv.y) / bend, 2.0);
    uv.y *= 1.0 + pow(abs(uv.x) / bend, 2.0);
    uv /= 2.0;
    return uv + 0.5;
}

float vignette(vec2 uv, float size, float smoothness, float edgeRounding)
{
    uv -= 0.5;
    uv *= size;
    float amount = sqrt(pow(abs(uv.x), edgeRounding) + pow(abs(uv.y), edgeRounding));
    return smoothstep(0.0, smoothness, 1.0 - amount);
}

vec4 get_color_bleeding(vec4 current_color, vec4 color_left)
{
    vec4 c = current_color * vec4(color_bleeding, 0.5, 1.0 - color_bleeding, 1.0);
    vec4 l = color_left * vec4(1.0 - color_bleeding, 0.5, color_bleeding, 1.0);
    return c + l;
}

float random(vec2 uv)
{
    return fract(sin(dot(uv * 0.05, vec2(12.9898, 78.233) * float(u_Time))) * 438.5453) * 1.9;
}

float noise(vec2 uv)
{
    vec2 i = floor(uv);
    vec2 f = fract(uv);

    float a = random(i);
    float b = random(i + vec2(1.0, 0.0));
    float c = random(i + vec2(0.0, 1.0));
    float d = random(i + vec2(1.0));

    vec2 u = smoothstep(0.0, 1.0, f);

    return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

/*
 * Lectura con vecino mas cercano: se lleva la coordenada al centro del texel. Asi el
 * escalado "nitido" no depende del estado de filtrado de la textura del framebuffer, que es
 * compartida con el resto del motor (GL_LINEAR).
 */
vec4 sample_point(vec2 uv)
{
    HIGHP vec2 pixel = floor(uv * TEX_SIZE) + 0.5;
    return texture(u_Texture0, pixel / TEX_SIZE);
}

/*
 * Escalado de calidad. Devuelve el color del pixel original sin tocarlo cuando los cuatro
 * vecinos son iguales (zonas planas, texto sobre fondo liso, degradados amplios): asi no se
 * pierde definicion. En los bordes mezcla en la direccion del borde.
 */
vec4 sample_smooth(vec2 uv)
{
    HIGHP vec2 inv = 1.0 / TEX_SIZE;
    HIGHP vec2 pos = uv * TEX_SIZE;

    vec4 c = sample_point(uv);
    vec4 n = texture(u_Texture0, uv + vec2(0.0, -inv.y));
    vec4 s = texture(u_Texture0, uv + vec2(0.0, inv.y));
    vec4 w = texture(u_Texture0, uv + vec2(-inv.x, 0.0));
    vec4 e = texture(u_Texture0, uv + vec2(inv.x, 0.0));

    /* Distancia al centro del texel, en el rango -0.5 .. 0.5 */
    HIGHP vec2 f = fract(pos) - 0.5;

    float diff = length(n.rgb - c.rgb) + length(s.rgb - c.rgb) + length(w.rgb - c.rgb) +
                 length(e.rgb - c.rgb);

    /* Zona plana: color exacto, sin emborronar. */
    if (diff < 0.15) {
        return c;
    }

    vec4 blend_h = mix(w, e, clamp(f.x + 0.5, 0.0, 1.0));
    vec4 blend_v = mix(n, s, clamp(f.y + 0.5, 0.0, 1.0));

    /* Se conserva el 35% del color original para no perder detalle. */
    return mix(mix(blend_h, blend_v, 0.5), c, 0.35);
}

void main()
{
    vec2 xy;
    vec4 col;

    if (c_curvature == 1) {
        xy = crt_coords(v_Texcoord0, Curvature);
        col = v_Color * vignette(xy, 1.9, 0.6, Curvature * 2.0);
    } else {
        xy = v_Texcoord0;
        col = v_Color;
    }

    vec4 color;
    if (c_noise == 1) {
        color = mix(col, vec4(noise(xy * 75.0)), 0.05);
    } else {
        color = col;
    }

    vec4 p;

    if (c_color_bleed == 1) {
        float pixel_size = 1.0 / float(c_WindowWidth) * bleeding_range;
        vec4 current_color = texture(u_Texture0, xy) * color;
        vec4 color_left = texture(u_Texture0, vec2(xy.x - pixel_size, xy.y)) * color;
        p = get_color_bleeding(current_color, color_left);
    } else if (c_upscale == 0) {
        p = sample_point(xy) * color;
    } else if (c_upscale == 1) {
        p = sample_smooth(xy) * color;
    } else {
        /*
         * Calidad maxima: se mezcla el escalado de calidad con tres muestras de vecino mas
         * cercano tomadas medio texel alrededor. Son 8 lecturas en total (no 20), que es lo
         * que permite hacerlo sin coste en un movil y quita el parpadeo de los bordes al
         * moverse la camara, que es lo que mas se nota al no estar alineado con la rejilla.
         */
        HIGHP vec2 inv = 0.5 / TEX_SIZE;
        vec4 base = sample_smooth(xy);
        vec4 a = sample_point(xy + vec2(-inv.x, -inv.y));
        vec4 b = sample_point(xy + vec2(inv.x, -inv.y));
        vec4 d = sample_point(xy + vec2(-inv.x, inv.y));
        p = mix(base, (base + a + b + d) * 0.25, 0.5) * color;
    }

    if (c_scanlines == 1) {
        float r = float(c_WindowHeight / 216);
        float f = mod(gl_FragCoord.y, r) / r;
        v_FragColor = mix(p, vec4(p.rgb * 0.5, p.a), f);
    } else {
        v_FragColor = p;
    }
}
