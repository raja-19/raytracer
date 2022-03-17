#version 430 core

layout (local_size_x = 1, local_size_y = 1) in;
layout (binding = 0, rgba32f) uniform image2D tex;

uniform vec3 eye, dir, up;

struct Sphere {
    vec3 center;
    float radius;
    int type;
    vec3 color;
};

struct Plane {
    vec3 point;
    vec3 normal;
};

const int n = 7;
vec3 bgcolor = vec3(0);

Sphere spheres[n] = {
    Sphere(vec3(0, 0, 3), 3, 2, vec3(0, 0, 0)),
    Sphere(vec3(6, 0, 5), 1, 2, vec3(0, 0, 0)),
    Sphere(vec3(-7, 0, 2), 1, 0, vec3(0, 1, 0)),
    Sphere(vec3(-6, -6, 3), 0.5, 1, vec3(0, 0, 0)),
    Sphere(vec3(-3, 7, 7), 1, 2, vec3(0, 0, 0)),
    Sphere(vec3(3, -8, 1.5), 1.5, 0, vec3(1, 0, 0)),
    Sphere(vec3(4, 7, 1), 1, 1, vec3(0, 0, 0)),
};

Plane plane = Plane(vec3(0), up);

float hitPlane(vec3 source, vec3 ray, Plane plane) {
    float b = dot(source - plane.point, plane.normal);
    float k = dot(ray, plane.normal);
    if (k == 0) return -1;
    float t = -b / k;
    return t;
}

float hitSphere(vec3 source, vec3 ray, Sphere sphere, int inside) {
    float a = dot(ray, ray);
    float b = 2 * dot(ray, source - sphere.center);
    float c = dot(source - sphere.center, source - sphere.center) - sphere.radius * sphere.radius;
    float D = b * b - 4 * a * c;
    if (D < 0) return -1;
    float t1 = (-b - sqrt(D)) / (2 * a);
    float t2 = (-b + sqrt(D)) / (2 * a);
    if (inside == 0 && t1 >= 0) return t1; 
    if (inside == 1 && t2 >= 0) return t2; 
    return -1;
}

vec3 getPlaneDiffuse(vec3 ray, vec3 point) {
    float val = sign(sin(2 * point.x) * sin(2 * point.y));
    return vec3(max(0, val));
}

vec3 getPlaneSpecular(vec3 viewpoint, vec3 ray, vec3 point) {
    vec3 n = plane.normal;
    vec3 viewray = normalize(viewpoint - point); 
    vec3 lightray = normalize(reflect(-up, n));
    float shininess = 64;
    float specular = pow(max(0, dot(viewray, lightray)), shininess);
    return vec3(0);
    return vec3(specular);
}

vec3 getSphereSpecular(vec3 viewpoint, vec3 ray, vec3 point, Sphere sphere) {
    vec3 n = normalize(point - sphere.center);
    vec3 viewray = normalize(viewpoint - point);
    vec3 lightray = normalize(reflect(-up, n));
    float shininess = 1 << 7;
    float specular = pow(max(0, dot(viewray, lightray)), shininess); 
    return vec3(specular);
}

vec3 getSphereDiffuse(vec3 ray, vec3 point, Sphere sphere) {
    vec3 color;
    if (sphere.type == 0) color = sphere.color;
    if (sphere.type == 1) color = point - sphere.center;
    vec3 n = normalize(point - sphere.center);
    float ambient = 0.1;
    float diffuse = max(0, dot(n, up));
    return (ambient + diffuse) * color;
}

vec3 getSphereReflection(vec3 ray, vec3 point, Sphere sphere) {
    vec3 specular = getSphereSpecular(eye, ray, point, sphere);
    for (int d = 0; d < 4; d++) {
        vec3 refray = normalize(reflect(ray, normalize(point - sphere.center)));
        float planet = hitPlane(point, refray, plane);
        if (length(point + refray * planet) > 10) planet = -1;
        float spheret = -1;
        int spherei = 0;
        for (int i = 0; i < n; i++) {
            if (sphere == spheres[i]) continue;
            float cur = hitSphere(point, refray, spheres[i], 0);
            if (cur >= 0 && (spheret < 0 || cur < spheret)) {
                spheret = cur;
                spherei = i;
            }
        } 
        if (planet >= 0 && (spheret < 0 || planet < spheret)) {
            float absorption = pow(0.5, d);
            vec3 refdiffuse = getPlaneDiffuse(refray, point + refray * planet);
            vec3 refspecular = getPlaneSpecular(point, refray, point + refray * planet);
            return absorption * (refdiffuse + refspecular) + specular;
        }
        if (spheret >= 0) {
            if (spheres[spherei].type == 0 || spheres[spherei].type == 1) {
                float absorption = pow(0.5, d);
                vec3 refdiffuse = getSphereDiffuse(refray, point + refray * spheret, spheres[spherei]);
                vec3 refspecular = getSphereSpecular(point, refray, point + refray * spheret, spheres[spherei]);
                return absorption * (refdiffuse + refspecular) + specular;
            }
            if (spheres[spherei].type == 2) {
                ray = refray;
                point = point + refray * spheret;
                sphere = spheres[spherei];
                continue;
            }
        }
    }
    return bgcolor;
}

vec3 getColor(vec3 ray) {
    float planet = hitPlane(eye, ray, plane);
    if (length(eye + ray * planet) > 10) planet = -1;
    float spheret = -1;
    int spherei = 0;
    for (int i = 0; i < n; i++) {
        float cur = hitSphere(eye, ray, spheres[i], 0);
        if (spheret < 0 || (cur >= 0 && cur < spheret)) {
            spheret = cur;
            spherei = i;
        }
    } 
    if (planet >= 0 && (spheret < 0 || planet < spheret)) {
        vec3 diffuse = getPlaneDiffuse(ray, eye + ray * planet);
        return diffuse;
    }
    if (spheret >= 0) {
        if (spheres[spherei].type == 0 || spheres[spherei].type == 1) {
            vec3 diffuse = getSphereDiffuse(ray, eye + ray * spheret, spheres[spherei]);
            vec3 specular = getSphereSpecular(eye, ray, eye + ray * spheret, spheres[spherei]);
            return diffuse + specular;
        }
        if (spheres[spherei].type == 2) {
            vec3 reflection = getSphereReflection(ray, eye + ray * spheret, spheres[spherei]);
            vec3 specular = getSphereSpecular(eye, ray, eye + ray * spheret, spheres[spherei]);
            return reflection + specular;
        }
    }
    return bgcolor;
}

void main() {
    ivec2 globalpos = ivec2(gl_GlobalInvocationID);
    vec2 coord = 2 * vec2(globalpos) / (1 << 10) - 1;

    vec3 i = normalize(cross(dir, up));
    vec3 j = normalize(cross(i, dir));
    vec3 ray = normalize(dir + i * coord.x + j * coord.y);

    vec3 color = getColor(ray);
    imageStore(tex, globalpos, vec4(color, 1));
}
