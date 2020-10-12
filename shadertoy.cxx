#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#define GL_GLEXT_PROTOTYPES
#include <GL/gl3w.h>
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstdlib>
#include <complex>

namespace imgui {
  // imgui attribute tags.
  using color  [[attribute]] = void;
  using checkbox [[attribute]] = void;

  // Center and size of render area.
  // using center [[attribute]] = void;
  // using area   [[attribute]] = void;

  template<typename type_t>
  struct minmax_t {
    type_t min, max;
  };
  using slider_float [[attribute]] = minmax_t<float>;
}

[[using spirv: in, location(0)]]
vec4 vertex_in;

[[spirv::vert]]
void vert_main() {
  glvert_Output.Position = vertex_in;
}

[[using spirv: out, location(0)]]
vec4 fragColor;

enum class uniform_location_t {
  resolution,
  time,
};

[[using spirv: uniform, location((int)uniform_location_t::resolution)]]
vec2 iResolution;

[[using spirv: uniform, location((int)uniform_location_t::time)]]
float iTime;

template<typename shader_t>
[[using spirv: uniform, binding(0)]]
shader_t shader_ubo;

template<typename shader_t>
[[spirv::frag]]
void frag_main() {
  fragColor = shader_ubo<shader_t>.render(glfrag_FragCoord.xy);
}

struct program_base_t {
  virtual void configure() { }

  GLuint program;
  GLuint ubo;
};

struct app_t {
  GLFWwindow* window;
  GLuint vao;
  GLuint array_buffer;

  app_t();
  void loop(program_base_t& program);

  static void debug_callback(GLenum source, GLenum type, GLuint id, 
    GLenum severity, GLsizei length, const GLchar* message, 
    const void* user_param);
};

void app_t::debug_callback(GLenum source, GLenum type, GLuint id, 
  GLenum severity, GLsizei length, const GLchar* message, 
  const void* user_param) {

  printf("OpenGL: %s\n", message);
  if(GL_DEBUG_SEVERITY_HIGH == severity ||
    GL_DEBUG_SEVERITY_MEDIUM == severity)
    exit(1);
}

app_t::app_t() {
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6); 
  glfwWindowHint(GLFW_STENCIL_BITS, 8);
  glfwWindowHint(GLFW_SAMPLES, 4); // HQ 4x multisample.
  glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);

  window = glfwCreateWindow(800, 800, "Circle does Shadertoy", 
    NULL, NULL);
  if(!window) {
    printf("Cannot create GLFW window\n");
    exit(1);
  }

  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  // Create an ImGui context.
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO(); (void)io;

  // Setup Platform/Renderer bindings
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 460");

  const float vertices[][2] { { -1, 1 }, { 1, 1 }, { -1, -1 }, { 1, -1 } };

  // Load into an array object.
  glCreateBuffers(1, &array_buffer);
  glNamedBufferStorage(array_buffer, sizeof(vertices), vertices, 0);

  // Declare a vertex array object and bind the array buffer.
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, array_buffer);

  // Bind to slot 0
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
  glEnableVertexAttribArray(0);
}

void app_t::loop(program_base_t& program) {

  while(!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    // Set the shadertoy uniforms.
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Configure the input program.
    program.configure();

    // Save the ImGui frame.
    ImGui::Render();

    // Bind and execute the input program.
    glUseProgram(program.program);
    glBindVertexArray(vao);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glUniform2f((int)uniform_location_t::resolution, width, height);

    glUniform1f((int)uniform_location_t::time, glfwGetTime());

    // Render the ImGui frame over the application.
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Swap buffers.
    glfwSwapBuffers(window);
  }
}


template<typename shader_t>
struct program_t : program_base_t {
  // Keep an instance of the shader parameters in memory to drive ImGui.
  shader_t shader;

  program_t();
  void configure() override;
};

template<typename shader_t>
program_t<shader_t>::program_t() {
  // Create vertex and fragment shader handles.
  GLuint vs = glCreateShader(GL_VERTEX_SHADER);
  GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
  GLuint shaders[] { vs, fs };

  // Associate shader handlers with the translation unit's SPIRV data.
  glShaderBinary(2, shaders, GL_SHADER_BINARY_FORMAT_SPIR_V_ARB, 
    __spirv_data, __spirv_size);
  glSpecializeShader(vs, @spirv(vert_main), 0, nullptr, nullptr);
  glSpecializeShader(fs, @spirv(frag_main<shader_t>), 0, nullptr, nullptr);

  // Link the shaders into a program.
  program = glCreateProgram();
  glAttachShader(program, vs);
  glAttachShader(program, fs);
  glLinkProgram(program);

  // Create the UBO.
  glCreateBuffers(1, &ubo);
  glNamedBufferStorage(ubo, sizeof(shader_t), nullptr, 
    GL_DYNAMIC_STORAGE_BIT);
}

template<typename shader_t>
void program_t<shader_t>::configure() {
  ImGui::Begin(@type_string(shader_t));

  using namespace imgui;
  @meta for(int i = 0; i < @member_count(shader_t); ++i) {{
    typedef @member_type(shader_t, i) type_t;
    const char* name = @member_name(shader_t, i);
    auto& value = @member_value(shader, i);

    if constexpr(@member_has_attribute(shader_t, i, color)) {
      ImGui::ColorEdit4(name, &value.x);

    } else if constexpr(@member_has_attribute(shader_t, i, slider_float)) {
      auto minmax = @member_attribute(shader_t, i, slider_float);
      ImGui::SliderFloat(name, &value, minmax.min, minmax.max);

    } else if constexpr(@member_has_attribute(shader_t, i, checkbox)) {
      ImGui::Checkbox(name, (bool*)&value);

    } else if constexpr(std::is_same_v<type_t, vec2>) {
      ImGui::DragFloat2(name, &value.x, .1f);
      
    } else if constexpr(std::is_same_v<type_t, float>) {
      ImGui::DragFloat(name, &value, .1f);
    }
  }}

  ImGui::End();


  glNamedBufferSubData(ubo, 0, sizeof(shader_t), &shader);
  glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo);
}


////////////////////////////////////////////////////////////////////////////////

inline vec2 rot(vec2 p, vec2 pivot, float a) {
  p -= pivot;
  p = vec2(
    p.x * cos(a) - p.y * sin(a), 
    p.x * sin(a) + p.y * cos(a)
  );
  p += pivot;

  return p;
}

inline vec2 rot(vec2 p, float a) {
  return rot(p, vec2(), a);
}


#if 0
struct mandelbrot_t {


  vec3 sine_color(float x) {
    return sin(vec3(.3, .45, .65) * x) * .5f + .5f;
  }

  // Run the shader.
  vec4 render(vec2 frag_coord) {
    // Normalize the area to the interval (0, 1)
    vec2 uv = frag_coord / iResolution;

    // Make the x dimension equal to scale. Make y res.y / res.x * scale.
    uv = (uv - .5f) * scale;
    uv.y *= iResolution.y / iResolution.x;

    uv += center;
    uv = rot(uv, center, angle);

    std::complex<float> z(uv.x, uv.y), c { };
    int iter;
    for(iter = 0; iter < 255; ++iter) {
      c = c * c + z;
      if(std::norm(c) > 4)
        break;
    }

    return vec4(sine_color(20 * sqrt(iter / 255.f)), 1);
  }

  // Data members with initializers.
   vec2  center      = vec2(-1, 0);
  float  scale                  = 3;
  [[.imgui::slider_float{-M_PI, M_PI}]] float angle = 0;
  [[.imgui::color]]              vec4  color_inner = vec4(1, 0, 0, 1);
  [[.imgui::color]]              vec4  color_outer = vec4(0, 1, 0, 1);
  [[.imgui::slider_float{0, 1}]] float radius      = .25f;
};
#endif 

struct modulation_t {
  // TODO: Import into spirv.cxx.
  float mod(float x, float y) {
    return x - y * floor(x / y);
  }

  // Signed distance to a n-star polygon with external angle en.
  float sdStar(vec2 p, float r, int n, float m) {
    float an = M_PIf32 / n;
    float en = M_PIf32 / m;
    vec2 acs(cos(an), sin(an));
    vec2 ecs(cos(en), sin(en));

    // reduce to first sector.
    float bn = mod(atan2(p.x, p.y), 2 * an) - an;
    p = length(p) * vec2(cos(bn), abs(sin(bn)));

    // line sdf
    p -= r * acs;
    p += ecs * clamp(-dot(p, ecs), 0.0f, r * acs.y / ecs.y);
    return length(p) * sign(p.x);
  }

  float sdShape(vec2 uv) {
    float angle = -iTime * StarRotationSpeed;
    return sdStar(rot(uv, angle), StarSize, StarPoints, StarWeight);
  }

  vec3 dtoa(float d, vec3 amount) {
    return 1 / clamp(d * amount, 1, amount);
  }

  // https://www.shadertoy.com/view/3t23WG
  // Distance to y(x) = a + b*cos(cx+d)
  float udCos(vec2 p, float a, float b, float c, float d) {
    p = c * (p - vec2(d, a));
    
    // Reduce to principal half cyc
    p.x = mod(p.x, 2 * M_PIf32);
    if(p.x > M_PIf32)
      p.x = 2 * M_PIf32 - p.x;

    // Fine zero of derivative (minimize distance).
    float xa = 0, xb = 2 * M_PIf32;
    for(int i = 0; i < 7; ++i) { // bisection, 7 bits more or less.
      float  x = .5f * (xa + xb);
      float si = sin(x);
      float co = cos(x);
      float  y = x - p.x + b * c * si * (p.y - b * c * co);
      if(y < 0)
        xa = x;
      else
        xb = x;
    }

    float x = .5f * (xa + xb);
    for(int i = 0; i < 4; ++i) { // Newton-Raphson, 28 bits more or less.
      float si = sin(x);
      float co = cos(x);
      float  f = x - p.x + b * c * (p.y * si - b * c * si * co);
      float df = 1       + b * c * (p.y * co - b * c * (2 * co * co - 1));
      x = x - f / df;
    }

    // Compute distance.
    vec2 q(x, b * c * cos(x));
    return length(p - q) / c;
  }

  vec4 render(vec2 frag_coord) {
    vec2 N = frag_coord / iResolution - .5f;
    vec2 uv = N;
    uv.x *= iResolution.x / iResolution.y;

    uv *= Zoom;
    float t = iTime * PhaseSpeed;

    vec2 uvsq = uv;
    float a = sdShape(uv);

    float sh = mix(100.f, 1000.f, Sharpness);

    float a2 = 1.5;
    for(int i = -3; i <= 3; ++i) {
      vec2 uvwave(uv.x, uv.y + i * WaveSpacing);
      float b = smoothstep(1.f, -1.f, a) * WaveAmp + WaveAmpOffset;
      a2 = min(a2, udCos(uvwave, 0.f, b, WaveFreq, t));
    }

    vec3 o = dtoa(mix(a2, a-LineWeight + 4, .03f), sh * Tint);
    if(!InvertColors)
      o = 1 - o;

    o *= 1 - dot(N, N * 2);
    return vec4(clamp(o, 0, 1), 1);
  }

                       float Zoom = 3;
                       float LineWeight = 4.3;

                       // TODO: Support bool in spirv.c
  [[.imgui::checkbox]] char InvertColors = true;   

                       float Sharpness = .2;
                     
                       float StarRotationSpeed = -.5;
                       float StarSize = 1.8;
                       int StarPoints = 3;
                       float StarWeight = 4;
                     
                       float WaveSpacing = .3;
                       float WaveAmp = .4;
                       float WaveFreq = 25;
                       float PhaseSpeed = .33;
                     
                       float WaveAmpOffset = .01;

  [[.imgui::color]] vec3 Tint = vec3(1, .5, .4);

};

int main() {
  glfwInit();
  gl3wInit();
  
  app_t app;

  program_t<modulation_t> modulation;
  app.loop(modulation);

  return 0;
}
