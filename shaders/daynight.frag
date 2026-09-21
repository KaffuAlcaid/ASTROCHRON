#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 viewSize;
    float pixelsPerDegree;
    float centerLongitude;
    float centerLatitude;
    vec3 sunDirection;
    vec4 nightColor;
    vec4 lineColor;
};

void main()
{
    vec2 point = (qt_TexCoord0 * viewSize - viewSize * 0.5) / pixelsPerDegree;
    float latitude = centerLatitude - point.y;
    if (abs(latitude) > 90.0) {
        fragColor = vec4(0.0);
        return;
    }
    float lon = radians(centerLongitude + point.x);
    float lat = radians(latitude);
    vec3 normal = vec3(cos(lat) * cos(lon), cos(lat) * sin(lon), sin(lat));
    float light = dot(normal, sunDirection);
    float width = max(fwidth(light) * 1.1, 0.00001);
    float line = 1.0 - smoothstep(0.0, width, abs(light));
    vec4 shade = light < 0.0 ? nightColor : vec4(0.0);
    fragColor = mix(shade, lineColor, line) * qt_Opacity;
}
