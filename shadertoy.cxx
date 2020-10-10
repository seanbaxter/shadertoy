#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#define GL_GLEXT_PROTOTYPES
#include <GL/gl3w.h>
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstdlib>

namespace imgui {
  // imgui attribute tags.
  using color [[attribute]] = void;

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

enum uniform_location_t {
  uniform_location_resolution,

};

[[using spirv: uniform, location(0)]]
vec2 iResolution;

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
    glUniform2f(uniform_location_resolution, width, height);

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
    if constexpr(@member_has_attribute(shader_t, i, color)) {
      ImGui::ColorEdit4(
        @member_name(shader_t, i), 
        &@member_value(shader, i).x
      );

    } else if constexpr(@member_has_attribute(shader_t, i, slider_float)) {
      auto minmax = @member_attribute(shader_t, i, slider_float);
      ImGui::SliderFloat(
        @member_name(shader_t, i),
        &@member_value(shader, i),
        minmax.min,
        minmax.max
      );
    }
  }}

  ImGui::End();


  glNamedBufferSubData(ubo, 0, sizeof(shader_t), &shader);
  glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo);
}


////////////////////////////////////////////////////////////////////////////////

struct fire_t {
  // Run the shader.
  vec4 render(vec2 frag_coord) {

    // Normalized pixel coordinates between -.5 and +.5.
    vec2 uv = (frag_coord - iResolution / 2) / iResolution;
    float r2 = dot(uv, uv);

    return r2 < radius * radius ? color_inner : color_outer;
  }

  // Data members with initializers.
  [[.imgui::color]]              vec4  color_inner = vec4(1, 0, 0, 1);
  [[.imgui::color]]              vec4  color_outer = vec4(0, 1, 0, 1);
  [[.imgui::slider_float{0, 1}]] float radius      = .25f;
};

int main() {
  glfwInit();
  gl3wInit();
  
  app_t app;

  program_t<fire_t> fire;
  app.loop(fire);

  return 0;
}
