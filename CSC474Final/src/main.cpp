// Core libraries
#include <iostream>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <thread>

// Third party libraries
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glad/glad.h>
#include <math.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Local headers
#include "GLSL.h"
#include "Program.h"
#include "WindowManager.h"
#include "Shape.h"
#include "Camera.h"
#include "line.h"
#include "ControlPoint.h"

#define MESHSIZE 100		// terrain
#define	FRAMES 61			// plane animation

using namespace std;
using namespace glm;

ofstream ofile;
string resourceDir = "../resources/";
//ifstream ifile_1;
int renderstate = 1;
int realspeed = 0;


double get_last_elapsed_time() {
	static double lasttime = glfwGetTime();
	double actualtime = glfwGetTime();
	double difference = actualtime - lasttime;
	lasttime = actualtime;
	return difference;
}

class Application : public EventCallbacks {
public:
	WindowManager *windowManager = nullptr;
    Camera *camera = nullptr;

    std::shared_ptr<Shape> shape, plane;
	std::shared_ptr<Program> phongShader, prog, heightshader, skyprog, linesshader, pplane;
    
    double gametime = 0;
    bool wireframeEnabled = false;
    bool mousePressed = false;
    bool mouseCaptured = false;
    glm::vec2 mouseMoveOrigin = glm::vec2(0);
    glm::vec3 mouseMoveInitialCameraRot;

    // terrain 
    GLuint VertexArrayID;
    GLuint MeshPosID, MeshTexID, IndexBufferIDBox;
    GLuint TextureID, Texture2ID, HeightTexID, AudioTex, AudioTexBuf;

   	// paths
	Line path1_render, campath_render, campath_inverse_render;
	vector<vec3> path1, campath, campath_inverse, cardinal, camcardinal, camcardinal_inverse;

	// pos, lookat, up - data
	vector<mat3> path1_controlpts, campath_controlpts;

	// toggle plane camera perspective
	int cam_persp = 0;		// toggle camera perspective 
	int back_count = 0; // keep track of how many nodes I added

    Application() {
        camera = new Camera();
    }
    
    ~Application() {
        delete camera;
    }

	void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods) {
		// Movement
        if (key == GLFW_KEY_W && action != GLFW_REPEAT) camera->vel.z = (action == GLFW_PRESS) * -0.2f;
        if (key == GLFW_KEY_S && action != GLFW_REPEAT) camera->vel.z = (action == GLFW_PRESS) * 0.2f;
        if (key == GLFW_KEY_A && action != GLFW_REPEAT) camera->vel.x = (action == GLFW_PRESS) * -0.2f;
        if (key == GLFW_KEY_D && action != GLFW_REPEAT) camera->vel.x = (action == GLFW_PRESS) * 0.2f;
        // Rotation
        if (key == GLFW_KEY_I && action != GLFW_REPEAT) camera->rotVel.x = (action == GLFW_PRESS) * 0.02f;
        if (key == GLFW_KEY_K && action != GLFW_REPEAT) camera->rotVel.x = (action == GLFW_PRESS) * -0.02f;
        if (key == GLFW_KEY_J && action != GLFW_REPEAT) camera->rotVel.y = (action == GLFW_PRESS) * 0.02f;
        if (key == GLFW_KEY_L && action != GLFW_REPEAT) camera->rotVel.y = (action == GLFW_PRESS) * -0.02f;
        if (key == GLFW_KEY_U && action != GLFW_REPEAT) camera->rotVel.z = (action == GLFW_PRESS) * 0.02f;
        if (key == GLFW_KEY_O && action != GLFW_REPEAT) camera->rotVel.z = (action == GLFW_PRESS) * -0.02f;
		if (key == GLFW_KEY_ENTER && action == GLFW_PRESS){
			vec3 dir,pos,up;
			camera->getUpRotPos(up, dir, pos);
			cout << "point position:" << pos.x << "," << pos.y<< "," << pos.z << endl;
			cout << "Zbase:" << dir.x << "," << dir.y << "," << dir.z << endl;
			cout << "Ybase:" << up.x << "," << up.y << "," << up.z << endl;
		}		
		if (key == GLFW_KEY_BACKSPACE && action == GLFW_PRESS) {
			vec3 dir, pos, up;
			camera->getUpRotPos(up, dir, pos);
			cout << endl;
			back_count++;
			cout << "backspace count: " << back_count << endl;
			cout << "point position:" << pos.x << "," << pos.y << "," << pos.z << endl;
			cout << "Zbase:" << dir.x << "," << dir.y << "," << dir.z << endl;
			cout << "Ybase:" << up.x << "," << up.y << "," << up.z << endl;
			cout << "point saved into ofile!" << endl << endl;
			cout << endl;
			ofile << "{{" << pos.x << "," << pos.y << "," << pos.z << "}," << endl;
			ofile << "{" << dir.x << "," << dir.y << "," << dir.z << "}," << endl;
			ofile << "{" << up.x << "," << up.y << "," << up.z << "}}," << endl;
			ofile << endl;
		}

       
        // Polygon mode (wireframe vs solid)
        if (key == GLFW_KEY_P && action == GLFW_PRESS) {
            wireframeEnabled = !wireframeEnabled;
            glPolygonMode(GL_FRONT_AND_BACK, wireframeEnabled ? GL_LINE : GL_FILL);
        }
        // Hide cursor (allows unlimited scrolling)
        if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
            mouseCaptured = !mouseCaptured;
            glfwSetInputMode(window, GLFW_CURSOR, mouseCaptured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
            resetMouseMoveInitialValues(window);
        }
        if (key == GLFW_KEY_TAB && action == GLFW_RELEASE)
		{
			cam_persp = !cam_persp;
		}
	}

	void mouseCallback(GLFWwindow *window, int button, int action, int mods) {
        mousePressed = (action != GLFW_RELEASE);
        if (action == GLFW_PRESS) {
            resetMouseMoveInitialValues(window);
        }
    }
    
    void mouseMoveCallback(GLFWwindow *window, double xpos, double ypos) {
        if (mousePressed || mouseCaptured) {
            float yAngle = (xpos - mouseMoveOrigin.x) / windowManager->getWidth() * 3.14159f;
            float xAngle = (ypos - mouseMoveOrigin.y) / windowManager->getHeight() * 3.14159f;
            camera->setRotation(mouseMoveInitialCameraRot + glm::vec3(-xAngle, -yAngle, 0));
        }
    }

	void resizeCallback(GLFWwindow *window, int in_width, int in_height) { }
    
    // Reset mouse move initial position and rotation
    void resetMouseMoveInitialValues(GLFWwindow *window) {
        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);
        mouseMoveOrigin = glm::vec2(mouseX, mouseY);
        mouseMoveInitialCameraRot = camera->rot;
    }

    void init_terrain_mesh()
    {

        //generate the VAO
        glGenVertexArrays(1, &VertexArrayID);
        glBindVertexArray(VertexArrayID);

        //generate vertex buffer to hand off to OGL
        glGenBuffers(1, &MeshPosID);
        glBindBuffer(GL_ARRAY_BUFFER, MeshPosID);
        glm::vec3 *vertices = new glm::vec3[MESHSIZE * MESHSIZE * 6];
        for (int x = 0; x < MESHSIZE; x++)
        {
            for (int z = 0; z < MESHSIZE; z++)
            {
                vertices[x * 6 + z*MESHSIZE * 6 + 0] = vec3(0.0, 0.0, 0.0) + vec3(x, 0, z);//LD
                vertices[x * 6 + z*MESHSIZE * 6 + 1] = vec3(1.0, 0.0, 0.0) + vec3(x, 0, z);//RD
                vertices[x * 6 + z*MESHSIZE * 6 + 2] = vec3(1.0, 0.0, 1.0) + vec3(x, 0, z);//RU
                vertices[x * 6 + z*MESHSIZE * 6 + 3] = vec3(0.0, 0.0, 0.0) + vec3(x, 0, z);//LD
                vertices[x * 6 + z*MESHSIZE * 6 + 4] = vec3(1.0, 0.0, 1.0) + vec3(x, 0, z);//RU
                vertices[x * 6 + z*MESHSIZE * 6 + 5] = vec3(0.0, 0.0, 1.0) + vec3(x, 0, z);//LU

            }
    
        }
        glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * MESHSIZE * MESHSIZE * 6, vertices, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
        delete[] vertices;
        //tex coords
        float t = 1. / MESHSIZE;
        vec2 *tex = new vec2[MESHSIZE * MESHSIZE * 6];
        for (int x = 0; x<MESHSIZE; x++)
            for (int y = 0; y < MESHSIZE; y++)
            {
                tex[x * 6 + y*MESHSIZE * 6 + 0] = vec2(0.0, 0.0)+ vec2(x, y)*t; //LD
                tex[x * 6 + y*MESHSIZE * 6 + 1] = vec2(t, 0.0)+ vec2(x, y)*t;   //RD
                tex[x * 6 + y*MESHSIZE * 6 + 2] = vec2(t, t)+ vec2(x, y)*t;     //RU
                tex[x * 6 + y*MESHSIZE * 6 + 3] = vec2(0.0, 0.0) + vec2(x, y)*t;    //LD
                tex[x * 6 + y*MESHSIZE * 6 + 4] = vec2(t, t) + vec2(x, y)*t;        //RU
                tex[x * 6 + y*MESHSIZE * 6 + 5] = vec2(0.0, t)+ vec2(x, y)*t;   //LU
            }
        glGenBuffers(1, &MeshTexID);
        //set the current state to focus on our vertex buffer
        glBindBuffer(GL_ARRAY_BUFFER, MeshTexID);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vec2) * MESHSIZE * MESHSIZE * 6, tex, GL_STATIC_DRAW);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
        delete[] tex;
        glGenBuffers(1, &IndexBufferIDBox);
        //set the current state to focus on our vertex buffer
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexBufferIDBox);
        GLuint *elements = new GLuint[MESHSIZE * MESHSIZE * 8];
    //  GLuint ele[10000];
        int ind = 0,i=0;
        for (i = 0; i<(MESHSIZE * MESHSIZE * 8); i+=8, ind+=6)
            {
            elements[i + 0] = ind + 0;
            elements[i + 1] = ind + 1;
            elements[i + 2] = ind + 1;
            elements[i + 3] = ind + 2;
            elements[i + 4] = ind + 2;
            elements[i + 5] = ind + 5;
            elements[i + 6] = ind + 5;
            elements[i + 7] = ind + 0;
            }           
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint)*MESHSIZE * MESHSIZE * 8, elements, GL_STATIC_DRAW);
        delete[] elements;
        glBindVertexArray(0);
    }

    void init_terrain_tex(const std::string& resourceDirectory) {

        int width, height, channels;
        char filepath[1000];

        //texture 1
        string str = resourceDirectory + "/sky.jpg";
        strcpy(filepath, str.c_str());
        unsigned char* data = stbi_load(filepath, &width, &height, &channels, 4);
        glGenTextures(1, &TextureID);
        glActiveTexture(GL_TEXTURE0);	// 1st texture unit - must set active unit to avoid overwriting textures
        glBindTexture(GL_TEXTURE_2D, TextureID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        //texture 2
        str = resourceDirectory + "/sky1.jpg";
        strcpy(filepath, str.c_str());
        data = stbi_load(filepath, &width, &height, &channels, 4);
        glGenTextures(1, &Texture2ID);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, Texture2ID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        //texture 3
        str = resourceDirectory + "/height.jpg";
        strcpy(filepath, str.c_str());
        data = stbi_load(filepath, &width, &height, &channels, 4);
        glGenTextures(1, &HeightTexID);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, HeightTexID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);


        //[TWOTEXTURES]
        //set the 2 textures to the correct samplers in the fragment shader:
        GLuint Tex1Location = glGetUniformLocation(prog->getPID(), "tex");//tex, tex2... sampler in the fragment shader
        GLuint Tex2Location = glGetUniformLocation(prog->getPID(), "tex2");
        // Then bind the uniform samplers to texture units:
        glUseProgram(prog->getPID());
        glUniform1i(Tex1Location, 0);
        glUniform1i(Tex2Location, 1);

        Tex1Location = glGetUniformLocation(heightshader->getPID(), "tex");//tex, tex2... sampler in the fragment shader
        Tex2Location = glGetUniformLocation(heightshader->getPID(), "tex2");
        // Then bind the uniform samplers to texture units:
        glUseProgram(heightshader->getPID());
        glUniform1i(Tex1Location, 0);
        glUniform1i(Tex2Location, 1);

        Tex1Location = glGetUniformLocation(skyprog->getPID(), "tex");//tex, tex2... sampler in the fragment shader
        Tex2Location = glGetUniformLocation(skyprog->getPID(), "tex2");
        // Then bind the uniform samplers to texture units:
        glUseProgram(skyprog->getPID());
        glUniform1i(Tex1Location, 0);
        glUniform1i(Tex2Location, 1);
        
        Tex1Location = glGetUniformLocation(linesshader->getPID(), "tex");//tex, tex2... sampler in the fragment shader
        Tex2Location = glGetUniformLocation(linesshader->getPID(), "tex2");
        // Then bind the uniform samplers to texture units:
        glUseProgram(linesshader->getPID());
        glUniform1i(Tex1Location, 0);
        glUniform1i(Tex2Location, 1);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    // would be cool if we could read the paths from a file instead?
	void initPlaneStuff(const std::string& resourceDirectory) {

		// init pplane ---------------------------------------------------

		plane = make_shared<Shape>();
		plane->loadMesh(resourceDirectory + "/ufo.obj");
		plane->resize();
		plane->init();

		// init control pts----------------------------------------------
		path1_controlpts = {
			{ { 1.82462,3.29561,-0.233744 },
			{ 0,0,1 },
			{ 0,1,0 } },

			{ { 1.77327,3.67437,6.98523 },
			{ -0.0581056,0.333271,0.941039 },
			{ 0.0558197,0.942241,-0.33025 } },

			{ { -0.257421,7.99375,24.5968 },
			{ 0.0276472,0.0511978,0.998306 },
			{ -0.267748,0.962575,-0.0419503 } },

			{ { 4.31041,6.11671,32.5695 },
			{ 0.10583,0.141937,0.984202 },
			{ 0.0317235,0.988775,-0.146007 } },

			{ { 0.386355,7.76873,47.0756 },
			{ 0.0650821,0.0960351,0.993248 },
			{ -0.352212,0.933506,-0.0671803 } },

			{ { -0.0401345,6.68862,59.9134 },
			{ -0.231314,0.050234,0.971581 },
			{ 0.327353,0.944454,0.0291047 } },

			{ { -1.92598,5.6219,68.2739 },
			{ -0.251569,0.052987,0.966388 },
			{ -0.0534665,0.996215,-0.0685408 } },

			{ { -3.15178,6.36157,82.2936 },
			{ 0.0343886,0.0512006,0.998096 },
			{ -0.267833,0.962628,-0.0401532 } },

			{ { 5.87815,-2.87101,100.994 },
			{ 0.0225866,-0.126085,0.991762 },
			{ 0.512447,0.853245,0.0968045 } },

			{ { 5.45264,-14.8414,114.917 },
			{ 0.438091,0.398094,0.805976 },
			{ 0.231761,0.816264,-0.52915 } },

			{ { 7.71238,-12.8161,126.139 },
			{ 0.19828,0.32605,0.924325 },
			{ 0.642936,0.668543,-0.373743 } },

			{ { 12.044,-12.8507,130.277 },
			{ 0.702771,-0.402906,0.586327 },
			{ 0.66591,0.662583,-0.342852 } },

			{ { 23.6486,-16.0552,140.294 },
			{ -0.422334,0.584797,0.692565 },
			{ 0.579886,0.761553,-0.289429 } },

			{ { 23.4832,-16.4937,144.45 },
			{ -0.380413,0.563665,0.73319 },
			{ 0.63494,0.735605,-0.236085 } },

			{ { 20.3474,-13.1944,152.32 },
			{ -0.751067,0.508018,0.421683 },
			{ 0.653497,0.662984,0.365232 } },

			// added -------
			{ { 17.8818,-12.2649,153.577 },
			{ -0.86855,0.0793259,0.489211 },
			{ 0.476182,0.407184,0.779392 } },

			{ { 13.9941,-11.28,154.563 },
			{ -0.987653,0.0552752,0.14658 },
			{ -0.0943801,-0.956761,-0.275138 } },
			// added -------

			{ { 7.89783,-9.84528,155.93 },
			{ -0.723903,0.330601,0.60553 },
			{ 0.0279987,0.891061,-0.45302 } },

			{ { 3.08793,-6.9524,162.217 },
			{ -0.0690312,0.330479,0.941286 },
			{ -0.30694,0.890731,-0.33524 } },

			{ { 2.54437,-0.446637,172.734 },
			{ -0.522136,0.0540937,0.851145 },
			{ 0.179041,0.9827,0.0473782 } },

			{ { -5.67223,-0.387834,177.216 },
			{ -0.864845,0.0535046,0.499179 },
			{ 0.159652,0.972,0.172419 } },

			{ { -9.88624,-3.67188,179.135 },
			{ -0.96088,-0.139229,0.239427 },
			{ -0.194227,0.955007,-0.224136 } },

			{ { -18.4215,-4.92565,181.678 },
			{ -0.45699,0.0542172,0.887818 },
			{ -0.209235,0.963579,-0.166544 } },

			{ { -27.4333,-4.93414,186.902 },
			{ -0.292482,0.149898,0.944449 },
			{ -0.210249,0.953391,-0.216429 } },

			{ { -34.8985,-1.89881,202.682 },
			{ -0.050093,-0.0427316,0.99783 },
			{ -0.263849,0.964156,0.0280438 } },

			{ { -36.3269,-0.974385,210.907 },
			{ 0.17662,-0.0427295,0.983351 },
			{ 0.265444,0.964109,-0.00578311 } },

			{ { -33.0315,-2.04749,235.693 },
			{ -0.229055,-0.0374337,0.972694 },
			{ 0.51295,0.84462,0.153297 } },

			{ { -40.4276,1.78672,255.586 },
			{ -0.562829,-0.0430371,0.825452 },
			{ -0.217882,0.971049,-0.097933 } },

			{ { -47.6629,4.69201,264.735 },
			{ -0.555985,-0.0401122,0.830224 },
			{ -0.3723,0.905054,-0.205595 } },

			{ { -52.7206,3.9252,279.906 },
			{ -0.782341,-0.0430305,0.621362 },
			{ -0.179399,0.970901,-0.15864 } },

			{ { -60.151,4.32976,287.531 },
			{ -0.781671,-0.0430305,0.622205 },
			{ -0.17957,0.970901,-0.158446 } },

			{ { -62.4053,4.15754,290.833 },
			{ -0.388207,-0.0430305,0.920567 },
			{ -0.23352,0.970901,-0.053093 } },

			{ { -68.4812,4.88884,293.643 },
			{ -0.579369,-0.0432595,0.813916 },
			{ 0.149279,0.976068,0.158139 } },

			{ { -79.895,4.87581,303.769 },
			{ 0.102289,-0.0424705,0.993848 },
			{ -0.277303,0.958266,0.0694907 } },

			{ { -76.7325,4.2523,314.962 },
			{ 0.550609,-0.0440023,0.833603 },
			{ 0.116942,0.992828,-0.0248351 } },

			{ { -68.1245,4.74186,324.136 },
			{ 0.516383,-0.240448,0.821909 },
			{ 0.224042,0.964276,0.141338 } },

			{ { -62.3375,2.73643,333.208 },
			{ 0.705116,-0.0441783,0.707715 },
			{ 0.109809,0.99282,-0.0474304 } },

			{ { -57.8755,2.45189,337.923 },
			{ 0.622721,-0.0423095,0.781299 },
			{ 0.265254,0.950823,-0.159927 } },

			{ { -56.2873,2.27573,341.466 },
			{ 0.171861,-0.0423095,0.984212 },
			{ 0.309456,0.950823,-0.0131625 } },

			{ { -56.8786,1.699,345.013 },
			{ -0.233356,-0.409515,0.881954 },
			{ 0.226492,0.859157,0.458858 } },

			{ { -57.3387,0.615257,347.029 },
			{ -0.324601,-0.647376,0.689593 },
			{ -0.0573571,0.741202,0.668827 } },

			{ { -58.5569,-1.56106,349.779 },
			{ -0.326951,-0.397206,0.857514 },
			{ 0.0419359,0.900391,0.433056 } },

			{ { -61.5104,-1.38842,357.256 },
			{ 0.451814,0.0427122,0.891089 },
			{ 0.139493,0.983185,-0.117854 } },

			{ { -56.9375,-0.228226,366.042 },
			{ 0.782465,0.331203,0.527308 },
			{ -0.164592,0.926704,-0.337829 } },

			{ { -40.9752,-2.59634,371.778 },
			{ 0.834442,-0.233985,0.498956 },
			{ 0.380969,0.899128,-0.215479 } },

			{ { -33.8685,-4.69812,375.154 },
			{ 0.799246,-0.322337,0.507252 },
			{ 0.459899,0.871366,-0.17092 } },

			{ { -21.7418,-11.7892,382.266 },
			{ 0.929377,-0.341894,0.139164 },
			{ 0.305755,0.924233,0.228709 } },

			{ { -7.5877,-17.3757,384.353 },
			{ 0.8325,-0.282648,-0.476502 },
			{ 0.329466,0.944038,0.0156342 } },

			{ { -3.07882,-19.1275,384.529 },
			{ 0.902488,-0.286543,-0.321572 },
			{ 0.285124,0.957046,-0.0525983 } },

			{ { 0.55176,-20.4066,383.889 },
			{ 0.812201,-0.469332,-0.346493 },
			{ 0.450798,0.881914,-0.137872 } },

			{ { 7.1201,-28.7223,381.795 },
			{ 0.595616,-0.77599,-0.207557 },
			{ 0.754039,0.629193,-0.188524 } },

			{ { 15.2214,-38.067,376.467 },
			{ -0.0763945,0.724988,-0.684512 },
			{ 0.14405,0.687341,0.711907 } },

			{ { 19.3992,-23.2532,365.961 },
			{ 0.391459,0.789804,-0.472195 },
			{ -0.447259,0.611763,0.652461 } },

			{ { 22.7718,-18.0812,359.715 },
			{ 0.32925,0.258848,-0.90807 },
			{ -0.0451035,0.964905,0.258696 } },

			{ { 27.1076,-15.3537,348.328 },
			{ 0.431281,0.257741,-0.864619 },
			{ -0.209191,0.960778,0.182059 } },

			{ { 50.7823,-7.62865,340.215 },
			{ 0.992633,0.000479574,0.121158 },
			{ 0.0295448,0.968847,-0.245892 } },


			{ { 63.1366,-5.50863,342.655 },
			{ 0.757833,0,0.652448 },
			{ 0.169877,0.965509,-0.197316 } },

			{ { 65.0123,-3.93096,348.497 },
			{ 0.776008,0.0606597,0.627799 },
			{ -0.147659,0.985175,0.0873281 } },


			{ { 68.4079,-3.76509,351.224 },
			{ 0.768165,0.0571941,0.637693 },
			{ -0.275048,0.928891,0.248012 } },

			{ { 82.4637,-4.54031,360.78 },
			{ 0.716211,0.0614556,0.695173 },
			{ -0.0469186,0.998102,-0.039897 } },

			{ { 91.161,-3.56835,373.433 },
			{ 0.0250038,0.0613815,0.997801 },
			{ 0.0477971,0.996898,-0.0625237 } },

			{ { 90.708,-2.34837,383.579 },
			{ -0.270541,0.0613815,0.96075 },
			{ 0.0641183,0.996898,-0.0456356 } },

			{ { 85.4762,-0.713395,399.243 },
			{ -0.454323,0.0612456,0.888729 },
			{ 0.101549,0.994692,-0.0166356 } },

			{ { 78.2671,2.66797,411.657 },
			{ -0.280492,0.0610348,0.957914 },
			{ -0.0953494,0.991268,-0.0910799 } },

			{ { 73.0576,3.29504,423.117 },
			{ 0.0142188,0.0610348,0.998034 },
			{ -0.117954,0.991268,-0.0589406 } },

			{ { 74.2134,3.38571,427.972 },
			{ 0.0281243,0.0610869,0.997736 },
			{ 0.107886,0.992115,-0.0637839 } },

			{ { 74.3056,3.32672,433.506 },
			{ -0.137893,0.0610869,0.988562 },
			{ 0.116978,0.992115,-0.0449894 } },

			{ { 72.1798,3.07836,449.448 },
			{ 0.128646,0.0607661,0.989827 },
			{ -0.1562,0.986904,-0.0402856 } },

			{ { 73.3354,6.15676,468.315 },
			{ 0.134625,0.0613755,0.988994 },
			{ -0.0590827,0.996801,-0.0538175 } },

			{ { 70.8265,4.61085,482.378 },
			{ 0.146073,-0.235958,0.960722 },
			{ -0.0166567,0.970414,0.240871 } },

			{ { 62.9619,-4.76501,519.424 },
			{ -0.399608,-0.235988,0.88579 },
			{ -0.0512521,0.970536,0.235444 } },

			{ { 58.3198,-9.88285,541.523 },
			{ -0.118611,0.208064,0.970897 },
			{ 0.0746082,0.976903,-0.200237 } },

			{ { 58.6521,-5.23854,565.998 },
			{ -0.129699,0.036503,0.990881 },
			{ 0.053039,0.998147,-0.0298282 } },

			{ { 59.0476,-7.25994,586.241 },
			{ -0.132836,0.03652,0.990465 },
			{ -0.0327602,0.998613,-0.0412141 } },

			{ { 59.3388,-10.2004,616.477 },
			{ -0.132836,0.03652,0.990465 },
			{ -0.0327602,0.998613,-0.0412141 } },
		};

		campath_controlpts = {
			{ { 0.627947,3.04489,-1.76117 },
			{ 0.0199478,0,0.999801 },
			{ -0.0599424,0.998201,0.00119596 } },

			{ { 2.02775,5.60918,0.894911 },
			{ 0.0314178,-0.197578,0.979784 },
			{ -0.0548081,0.978452,0.199067 } },

			{ { 2.58899,6.95066,7.81718 },
			{ 0.0198816,0.00110221,0.999802 },
			{ -0.0599644,0.998201,9.19807e-05 } },

			{ { 2.30572,7.00407,22.2186 },
			{ 0.0198433,0.00174105,0.999802 },
			{ -0.0599771,0.9982,-0.000547882 } },

			{ { 3.47108,7.07932,30.6937 },
			{ 0.0198433,0.00174105,0.999802 },
			{ -0.0599771,0.9982,-0.000547882 } },

			{ { 2.87553,6.97875,45.7323 },
			{ -0.0601031,0,0.998192 },
			{ 0.100313,0.994938,0.00604007 } },

			{ { 1.71378,7.82282,62.5198 },
			{ -0.343916,-0.267661,0.900044 },
			{ 0.105494,0.941431,0.320279 } },

			{ { -1.73852,7.11904,76.7815 },
			{ 0.175887,-0.15474,0.972172 },
			{ 0.0275486,0.987955,0.152268 } },

			{ { -0.846522,-0.165793,96.2927 },
			{ 0.635758,-0.439206,0.634752 },
			{ 0.310811,0.898387,0.310319 } },

			{ { 1.9619,-14.7381,111.103 },
			{ 0.774486,-0.00783723,0.632542 },
			{ 0.00607001,0.999969,0.00495753 } },

			{ { -5.91349,-15.3636,131.323 },
			{ 0.997768,-0.00782875,-0.0663205 },
			{ 0.00471795,0.998887,-0.0469329 } },

			{ { 12.4531,-13.037,147.99 },
			{ 0.476686,-0.107146,-0.872519 },
			{ 0.185567,0.982443,-0.0192637 } },

			{ { 16.7546,-11.1137,167.987 },
			{ -0.103647,0,-0.994614 },
			{ 0,1,0 } },

			{ { 16.7637,-6.13683,172.943 },
			{ -0.140755,-0.194433,-0.970765 },
			{ 0.172949,0.960621,-0.217478 } },

			{ { 16.7812,-10.0142,179.037 },
			{ -0.224939,-0.0767242,-0.971347 },
			{ 0.495797,0.849177,-0.181888 } },

			{ { 16.7812,-10.0142,179.037 },
			{ -0.224939,-0.0767242,-0.971347 },
			{ 0.495797,0.849177,-0.181888 } },
			//
			{ { 17.7379,-9.524,183.979 },
			{ -0.153326,-0.0675829,-0.985862 },
			{ 0.244487,0.964047,-0.104111 } },

			{ { 17.7379,-9.524,183.979 },
			{ -0.153326,-0.0675829,-0.985862 },
			{ 0.244487,0.964047,-0.104111 } },

			{ { 9.30089,-7.17519,186.355 },
			{ -0.115145,0.00297587,-0.993344 },
			{ 0.255616,0.966409,-0.026735 } },


			{ { 9.30089,-7.17519,186.355 },
			{ -0.532389,0.260686,-0.80536 },
			{ 0.360633,0.93059,0.0628224 } },

			{ { 2.67619,-1.17244,177.437 },
			{ -0.962867,0.0671598,0.261489 },
			{ -0.003039,0.965807,-0.259245 } },

			{ { -6.61611,-2.90613,178.456 },
			{ -0.906511,0.0671598,0.416805 },
			{ -0.0457805,0.965807,-0.255189 } },

			{ { -14.5176,-3.55059,180.454 },
			{ -0.871993,-0.0280285,0.488715 },
			{ -0.236083,0.898662,-0.369692 } },

			{ { -23.0146,-3.45324,182.948 },
			{ -0.721699,-0.023089,0.691821 },
			{ -0.477419,0.74029,-0.473331 } },

			{ { -32.0732,-3.03451,197.198 },
			{ -0.345172,0.0964537,0.93357 },
			{ 0.0334491,0.995337,-0.0904681 } },

			{ {-37.3106, -0.9474, 205.46},
			{ 0.243696,-0.102436,0.964427 },
			{ 0.0250953,0.99474,0.0993148 }},

			{ { -34.204,-1.95315,228.104 },
			{ 0.0392662,-0.10191,0.994018 },
			{ 0.105679,0.98963,0.0972857 } },

			{ { -46.8861,2.15174,245.066 },
			{ 0.330824,-0.10191,0.938173 },
			{ 0.12968,0.98963,0.0617712 } },

			{ { -48.7325,2.96305,250.1 },
			{ 0.114524,0,0.99342 },
			{ 0,1,0 } },

			{ { -54.9289,4.97914,266.526 },
			{ -0.14253,-0.10191,0.98453 },
			{ 0.0861813,0.98963,0.114914 } },

			// added
			{ { -60.0337,3.62931,276.137 },
			{ -0.0722893,0.199412,0.977246 },
			{ 0.0147108,0.979916,-0.198869 } },

			{ { -62.8642,4.09773,279.625 },
			{ -0.0682434,0.199412,0.977537 },
			{ 0.0138875,0.979916,-0.198928 } },

			{ { -65.2417,4.35663,283.06 },
			{ -0.169809,0.254612,0.952018 },
			{ 0.0447087,0.967043,-0.250656 } },

			{ { -71.3424,6.39618,288.495 },
			{ -0.0565036,0.0938535,0.993981 },
			{ 0.00532657,0.995586,-0.0937023 } },

			{ { -78.3445,5.9148,294.414 },
			{ 0.3064,0.0919529,0.947451 },
			{ -0.219045,0.975424,-0.0238299 } },

			{ { -84.6953,2.98731,304.702 },
			{ 0.73972,0.194515,0.644188 },
			{ -0.276701,0.960557,0.0276923 } },

			{ { -83.6484,0.910715,318.511 },
			{ 0.798708,0.194515,0.569411 },
			{ -0.272711,0.960557,0.0543968 } },

			{ { -72.3727,3.92947,332.66 },
			{ 0.886542,-0.230829,0.40095 },
			{ 0.21032,0.972994,0.0951198 } },

			{ { -68.8862,2.97153,336.577 },
			{ 0.901922,-0.0433601,0.429717 },
			{ 0.0218598,0.998255,0.0548469 } },

			{ { -65.9272,2.73084,339.649 },
			{ 0.902208,-0.0433601,0.429116 },
			{ 0.0218963,0.998255,0.0548323 } },

			{ { -64.7378,1.46447,341.859 },
			{ 0.696458,-0.164543,0.698478 },
			{ 0.0873151,0.985556,0.145108 } },

			{ { -62.9969,0.0843323,345.462 },
			{ 0.456131,-0.164543,0.874568 },
			{ 0.0400046,0.985556,0.16456 } },

			{ { -62.5536,0.0230342,351.805 },
			{ 0.441549,-0.164384,0.88205 },
			{ 0.127348,0.984603,0.119747 } },

			{ { -60.402,0.349063,363.858 },
			{ 0.871489,-0.161718,0.462983 },
			{ 0.0504903,0.968636,0.243301 } },

			{ { -62.925,-0.0712712,369.615 },
			{ 0.993125,-0.0566601,-0.102429 },
			{ 0.0563612,0.998394,-0.005813 } },

			{ { -49.5347,-2.20714,376.184 },
			{ 0.993125,-0.0566601,-0.102429 },
			{ 0.0563612,0.998394,-0.005813 } },

			{ { -35.7021,-2.89615,370.104 },
			{ 0.68809,-0.348118,0.636668 },
			{ 0.255519,0.937451,0.236424 } },

			{ { -29.1288,-8.56831,374.781 },
			{ 0.786671,-0.247106,0.565763 },
			{ 0.200612,0.968988,0.144278 } },

			{ { -18.0004,-13.3408,377.743 },
			{ 0.883592,-0.247106,0.39775 },
			{ 0.225328,0.968988,0.101432 } },
			{ { -8.375,-18.2241,381.461 },
			{ 0.944752,-0.301802,0.127901 },
			{ 0.299074,0.953371,0.0404886 } },

			{ { -7.22022,-18.2241,378.033 },
			{ 0.850916,-0.301802,0.42995 },
			{ 0.269368,0.953371,0.136106 } },

			{ { -2.32307,-21.4091,369.053 },
			{ 0.635569,-0.427619,0.642801 },
			{ 0.40307,0.893906,0.19613 } },

			{ { 17.4619,-15.5051,354.724 },
			{ 5.74322e-05,-0.561295,0.827616 },
			{ 3.89509e-05,0.827616,0.561295 } },

			{ { 25.0567,-12.3742,348.53 },
			{ -0.270537,-0.557171,0.785093 },
			{ -0.29369,0.824395,0.483859 } },

			{ { 28.2645,-9.65866,338.111 },
			{ -0.038546,-0.471335,0.881112 },
			{ -0.133373,0.876304,0.462928 } },

			{ { 30.4868,-5.99006,328.55 },
			{ -0.0391072,-0.471335,0.881087 },
			{ -0.133668,0.876304,0.462843 } },

			{ { 37.2137,-3.35305,327.97 },
			{ -0.378109,-0.471335,0.796792 },
			{ -0.302875,0.876304,0.374643 } },

			{ { 83.2932,0.343353,335.399 },
			{ -0.896378,-0.198599,0.396315 },
			{ -0.181638,0.980081,0.0803075 } },

			{ { 84.3095,-5.21159,361.056 },
			{ -0.822283,0.206301,-0.530369 },
			{ 0.171807,0.978484,0.114239 } },

			{ { 91.4316,-5.30275,368.632 },
			{ -0.73289,0.206302,-0.648315 },
			{ 0.152711,0.978485,0.138733 } },

			{ { 93.2046,-3.75314,379.23 },
			{ -0.367208,0.197265,-0.90898 },
			{ 0.347386,0.935623,0.0627099 } },

			{ { 86.8592,-1.5845,393.683 },
			{ 0.314786,0,-0.949163 },
			{ 0.100981,0.994325,0.0334899 } },

			{ { 80.5374,1.08013,409.269 },
			{ 0.483034,-0.0250638,-0.875243 },
			{ 0.309113,0.94011,0.143674 } },

			{ { 70.7933,3.79463,425.595 },
			{ 0.720065,-0.0250638,-0.693454 },
			{ 0.252864,0.94011,0.228589 } },

			{ { 64.4954,4.11044,435.906 },
			{ 0.892789,-0.0250638,-0.449778 },
			{ 0.174047,0.94011,0.293088 } },

			{ { 67.3831,4.19236,441.633 },
			{ 0.892789,-0.0250638,-0.449778 },
			{ 0.174047,0.94011,0.293088 } },

			{ { 72.6966,2.88157,444.291 },
			{ 0.733321,-0.0266508,0.67936 },
			{ 0.0224268,0.999636,0.0150069 } },

			{ { 72.8772,2.82504,446.422 },
			{ -0.0688268,-0.0254893,0.997303 },
			{ -0.293116,0.956068,0.00420662 } },

			{ { 71.9083,3.18824,456.089 },
			{ -0.141091,0.201726,0.969226 },
			{ -0.0181182,0.97833,-0.206258 } },

			{ { 71.9537,6.34059,473.961 },
			{ -0.0699407,-0.350523,0.933939 },
			{ -0.110897,0.93316,0.341926 } },

			{ { 65.4211,-2.39831,512.209 },
			{ -0.254294,-0.350523,0.90137 },
			{ -0.176687,0.93316,0.313039 } },

			{ { 71.6129,-2.65166,524.432 },
			{ -0.775221,-0.350523,0.525515 },
			{ -0.336814,0.93316,0.125569 } },

			{ { 78.1184,-2.87367,560.331 },
			{ -0.942391,-0.0958883,0.320475 },
			{ -0.0746554,0.99416,0.0779272 } },

			{ { 78.157,-4.49958,581.615 },
			{ -0.99502,-0.0958883,0.0272049 },
			{ -0.0943744,0.99416,0.0523407 } },

			{ { 82.3097,-7.12355,609.453 },
			{ -0.927113,-0.0958883,-0.362307 },
			{ -0.107306,0.99416,0.0114712 } },
			};

		// init line ----------------------------------------------------
		// path1
		path1_render.init();
		for (int i = 0; i < path1_controlpts.size(); i++) {
			path1_controlpts[i] *= -1.0f;
			path1.push_back(path1_controlpts[i][0]);
			//	cout << path1_controlpts[i][0].x << " " << path1_controlpts[i][0].y << " " << path1_controlpts[i][0].z << endl;
		}
		path1_render.re_init_line(path1);
		cout << "path 1 has: " << path1.size() << " points\n" << endl;

		cardinal_curve(cardinal, path1, FRAMES, 1.0);
		path1_render.re_init_line(cardinal);

		// campath
		campath_render.init();
		for (int i = 0; i < campath_controlpts.size(); i++) {
			campath.push_back(campath_controlpts[i][0]);
			campath_inverse.push_back(campath_controlpts[i][0] * -1.0f);
			//cout << "campath: " << campath_controlpts[i][0].x << " " << campath_controlpts[i][0].y << " " << campath_controlpts[i][0].z << endl;
		}
		campath_render.re_init_line(campath);
		cardinal_curve(camcardinal, campath, FRAMES, 1.0);
		campath_render.re_init_line(camcardinal);
		cout << "cam path has: " << campath.size() << " points" << endl;

		// campath - inverse (drawing purposes)
		campath_inverse_render.init();
		campath_inverse_render.re_init_line(campath_inverse);
		cardinal_curve(camcardinal_inverse, campath_inverse, FRAMES, 1.0);
		campath_inverse_render.re_init_line(camcardinal_inverse);
	}



	void initGeom(const std::string& resourceDirectory) {
		init_terrain_mesh();

        shape = make_shared<Shape>();
        shape->loadMesh(resourceDirectory + "/sphere.obj");
        shape->resize();
        shape->init();

        init_terrain_tex(resourceDirectory);

        initPlaneStuff(resourceDirectory);

	}
	
	void init(const std::string& resourceDirectory) {
		GLSL::checkVersion();

		// Enable z-buffer test.
		glEnable(GL_DEPTH_TEST);
        
		// Initialize the GLSL programs
        phongShader = std::make_shared<Program>();
        phongShader->setShaderNames(resourceDirectory + "/phong.vert", resourceDirectory + "/phong.frag");
        phongShader->init();

        skyprog = std::make_shared<Program>();
        skyprog->setShaderNames(resourceDirectory + "/sky.vert", resourceDirectory + "/sky.frag");
        skyprog->init();

        prog = std::make_shared<Program>();
        prog->setShaderNames(resourceDirectory + "/shader.vert", resourceDirectory + "/shader.frag");
        prog->init();

        heightshader = std::make_shared<Program>();
        heightshader->setShaderNames(resourceDirectory + "/height.vert", resourceDirectory + "/height.frag", resourceDirectory + "/height.geom");
        heightshader->init();

        linesshader = std::make_shared<Program>();
        linesshader->setShaderNames(resourceDirectory + "/lines_height.vert", resourceDirectory + "/lines_height.frag", resourceDirectory + "/lines_height.geom");
        linesshader->init();

        pplane = std::make_shared<Program>();
        pplane->setShaderNames(resourceDirectory + "/plane.vert", resourceDirectory + "/plane.frag");
        pplane->init();

	}
    
    glm::mat4 getPerspectiveMatrix() {
        float fov = 3.14159f / 4.0f;
        float aspect = windowManager->getAspect();
        return glm::perspective(fov, aspect, 0.01f, 10000.0f);
    }

    mat4 linint_between_two_orientations(vec3 ez_lookto_1, vec3 ey_up_1, vec3 ez_lookto_2, vec3 ey_up_2, float t) {
		mat4 m1, m2;
		quat q1, q2;
		vec3 ex, ey, ez;

		t = ((-cos(t* 3.14)) + 1) / 2.0;	// smooth transition

		// get rotation matrix ---------
		ey = ey_up_1;											// control point 1
		ez = ez_lookto_1;
		ex = cross(ey, ez);
		m1[0][0] = ex.x;	m1[1][0] = ey.x;	m1[2][0] = ez.x;	m1[3][0] = 0;
		m1[0][1] = ex.y;	m1[1][1] = ey.y;	m1[2][1] = ez.y;	m1[3][1] = 0;
		m1[0][2] = ex.z;	m1[1][2] = ey.z;	m1[2][2] = ez.z;	m1[3][2] = 0;
		m1[0][3] = 0;		m1[1][3] = 0;		m1[2][3] = 0;		m1[3][3] = 1.0f;
		ey = ey_up_2;											// control point 2
		ez = ez_lookto_2;
		ex = cross(ey, ez);
		m2[0][0] = ex.x;	m2[1][0] = ey.x;	m2[2][0] = ez.x;	m2[3][0] = 0;
		m2[0][1] = ex.y;	m2[1][1] = ey.y;	m2[2][1] = ez.y;	m2[3][1] = 0;
		m2[0][2] = ex.z;	m2[1][2] = ey.z;	m2[2][2] = ez.z;	m2[3][2] = 0;
		m2[0][3] = 0;		m2[1][3] = 0;		m2[2][3] = 0;		m2[3][3] = 1.0f;

		// Convert to quarternion -------
		q1 = quat(m1);
		q2 = quat(m2);

		// Interpolate ------------------
		quat qt = slerp(q1, q2, t);
		qt = normalize(qt);

		// Convert back to Matrix -------
		mat4 mt = mat4(qt);

		return mt;
	}

	mat4 setupObjAlongPath(float frametime,  vector<vec3> path, vector<mat3> controlpts) {
		mat4 TransPlane1, RotPlane1;
		// Translate Plane Along Path
		static float sumft = 0; // sum of frame times
		sumft += frametime;
		float f = sumft * FRAMES;
		int frame = f;
		if (frame >= path.size() - 1) {								// loop through path
			sumft = 0;
			frame = 0;
		}
	
		// Rotate Plane Along Path
		vec3 ez1, ey1, ez2, ey2;							// zbase and ybase vectors for two contorl points
		static float t = 0.0;								// t for interpoltation
		t = (float)(frame % (FRAMES - 1)) / (float)(FRAMES - 1);

		ez1 = ez2 = controlpts[frame / (FRAMES - 1)][1];				// ez1 - up
		ey1 = ey2 = controlpts[frame / (FRAMES - 1)][2];				// ey1 - lookat

		if ((frame / (FRAMES - 1)) + 1 < controlpts.size()) {		// check if the next control pt exists
			ez2 = controlpts[(frame / (FRAMES - 1)) + 1][1];		// ez2 - up
			ey2 = controlpts[(frame / (FRAMES - 1)) + 1][2];		// ey2 - lookat
		}
		RotPlane1 = linint_between_two_orientations(ez1, ey1, ez2, ey2, t);
		TransPlane1 = glm::translate(glm::mat4(1.0f), path[frame]);

		return TransPlane1 * RotPlane1;
	}

	mat4 CamPathView(float frametime) {
		// Translate Plane Along Path
		static float sumft = 0; // sum of frame times
		sumft += frametime;
		float f = sumft * FRAMES;
		int frame = f;
		if (frame >= camcardinal.size() - 1) {								// loop through path
			sumft = 0;
			frame = 0;
		}

		camera->pos = camcardinal[frame];

		float t = 0.0;
		t = (float)(frame % (FRAMES - 1)) / (float)(FRAMES - 1);
		vec3 ez1, ey1, ez2, ey2;
		ez1 = ez2 = campath_controlpts[frame / (FRAMES - 1)][1];				// ez1 - up
		ey1 = ey2 = campath_controlpts[frame / (FRAMES - 1)][2];				// ey1 - lookat

		if ((frame / (FRAMES - 1)) + 1 < campath_controlpts.size()) {		// check if the next control pt exists
			ez2 = campath_controlpts[(frame / (FRAMES - 1)) + 1][1];		// ez2 - up
			ey2 = campath_controlpts[(frame / (FRAMES - 1)) + 1][2];		// ey2 - lookat
		}

		mat4 RotPlane = linint_between_two_orientations(ez1, ey1, ez2, ey2, t);
		mat4 TransPlane = glm::translate(mat4(1.0), camcardinal[frame]);
		return transpose(RotPlane) * TransPlane;
	}

	void render() {
		double frametime = get_last_elapsed_time();
		gametime += frametime;

		// Clear framebuffer.
		glClearColor(0.3f, 0.7f, 0.8f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Create the matrix stacks.
		glm::mat4 V, M, P;
        P = getPerspectiveMatrix();
        V = camera->getViewMatrix();		
		if (cam_persp) {
			V = CamPathView(frametime);
		}

        M = glm::mat4(1);
        
        /*************** DRAW SHAPE ***************
        M = glm::translate(glm::mat4(1), glm::vec3(0, 0, -3));
        phongShader->bind();
        phongShader->setMVP(&M[0][0], &V[0][0], &P[0][0]);
        shape->draw(phongShader, false);
        phongShader->unbind();
        */

        /************* draw sky sphere ******************/
		static float w = 0.6;
        glm::mat4 RotateY = glm::rotate(glm::mat4(1.0f), w, glm::vec3(0.0f, 1.0f, 0.0f));
        float angle = 3.1415926 / 2.0;
        glm::mat4 RotateX = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(1.0f, 0.0f, 0.0f));
        glm::mat4 TransZ = glm::translate(glm::mat4(1.0f), -camera->pos);
        glm::mat4 S = glm::scale(glm::mat4(1.0f), glm::vec3(0.8f, 0.8f, 0.8f));
        M = TransZ *RotateY * RotateX * S;

        skyprog->bind();
        skyprog->setMVP(&M[0][0], &V[0][0], &P[0][0]);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, TextureID);
		glDisable(GL_DEPTH_TEST);
        shape->draw(skyprog,false);
		glEnable(GL_DEPTH_TEST);
        skyprog->unbind();

        /************* draw terrain ******************/
        glm::mat4 TransY = glm::translate(glm::mat4(1.0f), glm::vec3(-50.0f, -9.0f, -50));
        M = TransY;

        vec3 offset = camera->pos;
			offset.y = 0; offset.x = (int)offset.x;	offset.z = (int)offset.z;
        vec3 bg = vec3(254. / 255., 225. / 255., 168. / 255.);
        if (renderstate == 2)
            bg = vec3(49. / 255., 88. / 255., 114. / 255.);
        
        heightshader->bind();
        heightshader->setMVP(&M[0][0], &V[0][0], &P[0][0]);
        glUniform3fv(heightshader->getUniform("camoff"), 1, &offset[0]);
        glUniform3fv(heightshader->getUniform("campos"), 1, &camera->pos[0]);
        glUniform3fv(heightshader->getUniform("bgcolor"), 1, &bg[0]);
        glUniform1i(heightshader->getUniform("renderstate"), renderstate);
        glBindVertexArray(VertexArrayID);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, HeightTexID);
        glDrawArrays(GL_TRIANGLES, 0, MESHSIZE*MESHSIZE * 6);           
        heightshader->unbind(); 
		
		//cout << camera->pos.x << " " << camera->pos.y << " " << camera->pos.z << " " << endl;
	
		// draw control points
		prog->bind();
		glUniformMatrix4fv(prog->getUniform("P"), 1, GL_FALSE, &P[0][0]);
		glUniformMatrix4fv(prog->getUniform("V"), 1, GL_FALSE, &V[0][0]);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, 0);

		int activate_red = 0;
		float size = 0.3, red = 0.0, green = 0.0;
		for (int i = 0; i < path1_controlpts.size(); i++) {
			S = scale(mat4(1.0), vec3(size));
			mat4 transCP = translate(mat4(1.0), path1_controlpts[i][0]);
			M = transCP * S;
			glUniformMatrix4fv(prog->getUniform("M"), 1, GL_FALSE, &M[0][0]);
			glUniform1f(prog->getUniform("red"), red);
			glUniform1f(prog->getUniform("green"), green);
			shape->draw(prog, false);

			if (activate_red) {
				red += 0.2;
			} else{
				green += 0.2;
			}
			if (red >= 1.0 || green >= 1.0) {
				activate_red = !activate_red;
				red = 0.0; green = 0.0;
			} 
		}

		activate_red = 0;
		size = 0.1, red = 0.0, green = 0.0;
		for (int i = 0; i < campath_controlpts.size(); i++) {
			mat4 transCP = translate(mat4(1.0), (campath_controlpts[i][0]*-1.0f));
			M = transCP * S;
			glUniformMatrix4fv(prog->getUniform("M"), 1, GL_FALSE, &M[0][0]);
			glUniform1f(prog->getUniform("red"), red);
			glUniform1f(prog->getUniform("green"), green);
			shape->draw(prog, false);

			if (activate_red) {
				red += 0.2;
			}
			else {
				green += 0.2;
			}
			if (red >= 1.0 || green >= 1.0) {
				activate_red = !activate_red;
				red = 0.0; green = 0.0;
			}
		}	

		// Draw the plane -------------------------------------------------------------------
		pplane->bind();

		glUniformMatrix4fv(pplane->getUniform("P"), 1, GL_FALSE, &P[0][0]);				// send constant attributes to shaders
		glUniformMatrix4fv(pplane->getUniform("V"), 1, GL_FALSE, &V[0][0]);
		glUniform3fv(pplane->getUniform("campos"), 1, &camera->pos[0]);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, Texture2ID);

		//plane 1
		// initalize transformation of plane
		glm::mat4 T = glm::translate(glm::mat4(1.0f), vec3(0, 0, -3));
		glm::mat4 ScalePlane = glm::scale(glm::mat4(1.0f), glm::vec3(1.5f));
		float sangle = -3.1415926 / 2.;
		glm::mat4 RotateXPlane = glm::rotate(glm::mat4(1.0f), sangle, vec3(1, 0, 0));
		sangle = 3.1415926;
		glm::mat4 RotateZPlane = glm::rotate(glm::mat4(1.0f), sangle, vec3(0, 0, 1));
		glm::mat4 RotateYPlane = glm::rotate(glm::mat4(1.0f), sangle, vec3(0, 1, 0));

		M =  T * setupObjAlongPath(frametime/2.0, cardinal, path1_controlpts)* RotateZPlane;
		glUniformMatrix4fv(pplane->getUniform("M"), 1, GL_FALSE, &M[0][0]);
		plane->draw(pplane, false);		

		pplane->unbind();

		// Draw the line path --------------------------------------------------------------
		
		glm::vec3 linecolor = glm::vec3(1, 0, 0);
		path1_render.draw(P, V, linecolor);

		linecolor = glm::vec3(0, 0, 0);
		campath_inverse_render.draw(P, V, linecolor);
	}
};

// testing control points
void testCPClass() {
	ControlPoint *CP = nullptr;
	CP = new ControlPoint();

	//CP->loadPoints(resourceDir + "path1.txt");
	CP->clearPoints(resourceDir + "path1.txt");

}

int main(int argc, char **argv) {

	// setup resource directory
	if (argc >= 2)
		resourceDir = argv[1];

	// open file to write path
	ofile.open(resourceDir + "pathinfo.txt");
	if (!ofile.is_open())
		cout << "warning! could not open pathinfo.txt file!" << endl;

	testCPClass();
	/*
	ifile_1.open("path1.txt");
	if (!ifile_1.is_open()) {
		cout << "warning! could not open path1.txt file!" << endl;
	}
	else {																	// load characters into vector array
		/*char c = ' ';
		while (file.get(c)) {
			charData.push_back(tolower(c));
			charData_Key.push_back(mapChar(c));									// map char to face using key
			cout << c;
		}
		cout << endl; 

		char* str = '\0';
		ifile_1.getline(str, 10, ' ');
	cout << "str: " << *str << endl;


	}*/

	Application *application = new Application();

    // Initialize window.
	WindowManager * windowManager = new WindowManager();
	windowManager->init(1280, 720);
	windowManager->setEventCallbacks(application);
	application->windowManager = windowManager;

	// Initialize scene.
	application->init(resourceDir);
	application->initGeom(resourceDir);
    
	// Loop until the user closes the window.
	while (!glfwWindowShouldClose(windowManager->getHandle())) {
        // Update camera position.
        application->camera->update();
		// Render scene.
		application->render();

		// Swap front and back buffers.
		glfwSwapBuffers(windowManager->getHandle());
		// Poll for and process events.
		glfwPollEvents();
	}

	// Quit program.
	windowManager->shutdown();
	return 0;
}
