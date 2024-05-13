
#include "framework.h"



// vertex shader in GLSL: It is a Raw string (C++11) since it contains new line characters

const char * const vertexSource = R"(

    #version 330

    precision highp float;        // normal floats, makes no difference on desktop computers



   uniform mat4 MVP;

   layout(location = 0) in vec2 vertexPosition;

   void main()

    {

        gl_Position = vec4(vertexPosition, 0, 1) * MVP;

    }

)";



// fragment shader in GLSL

const char * const fragmentSource = R"(

    #version 330

    precision highp float;    // normal floats, makes no difference on desktop computers



    uniform vec3 color;

    out vec4 fragmentColor;

    void main()

    {

        fragmentColor = vec4(color, 1);

    }
)";
using namespace std;
GPUProgram gpuProgram; // vertex and fragment shaders

const int parts = 100;// virtual world on the GPU

class Camera2D {
    vec2 wCenter;// center in world coords
    vec2 wSize; // width and height in world coords
public:
    Camera2D(): wCenter(0,0), wSize(30,30){};
    mat4 V() { return TranslateMatrix(-wCenter); }
    mat4 P()
    { // projection matrix
        return ScaleMatrix(vec2(2/wSize.x, 2/wSize.y));
    }
    mat4 Vinv()
    { // inverse view matrix
        return TranslateMatrix(wCenter);
    }
    mat4 Pinv()
    { // inverse projection matrix
        return ScaleMatrix(vec2(wSize.x/2, wSize.y/2));
    }
    void Zoom(float s) { wSize = wSize * s; }
    void Pan(vec2 t) { wCenter = wCenter + t; }
};

Camera2D camera;

class Curve
{
    unsigned int vao_curve, vbo_curve; // GPU
    unsigned int vao_point, vbo_point;
protected:
    vector<vec2> cps;
public:
    Curve()
    {
        glGenVertexArrays(1, &vao_curve);
        glBindVertexArray(vao_curve);
        glGenBuffers(1, &vbo_curve);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_curve);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vec2),NULL);

        glGenVertexArrays(1, &vao_point);
        glBindVertexArray(vao_point);
        glGenBuffers(1, &vbo_point);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_point);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vec2),NULL);
    }

    ~Curve()
    {
        glDeleteBuffers(1,&vbo_curve);
        glDeleteVertexArrays(1, &vao_curve);
        glDeleteBuffers(1, &vbo_point);
        glDeleteVertexArrays(1,&vao_point);
    }
    virtual void SetTension(float initialTension) {};
    virtual void AddControlPoint(vec2 cp)
    {
        vec4 ver = vec4(cp.x,cp.y,0,1) * camera.Pinv()*camera.Vinv();
        cps.push_back(vec2(ver.x,ver.y));
    }
    int findPoint(vec2 p)
    {
        vec4 ver = vec4(p.x,p.y,0,1) * camera.Pinv()*camera.Vinv();
        for(int i=0;i<cps.size();i++)
        {
            if (length(cps[i]-vec2(ver.x,ver.y))<0.1)
            {
                return i;
            }
        }
        return -1;
    }
    void movePoint(int i,  vec2 p)
    {
        vec4 ver = vec4(p.x,p.y,0,1) * camera.Pinv()*camera.Vinv();
        cps[i] = vec2(ver.x,ver.y);
    }
    virtual vec2 r(float t) = 0;
    virtual float start()=0;
    virtual float end()=0;
    void Draw()
    {
        mat4 MVP = camera.V()*camera.P();
        gpuProgram.setUniform(MVP, "MVP");
        if (cps.size()>0)
        {
            glBindVertexArray(vao_point);
            glBindBuffer(GL_ARRAY_BUFFER, vbo_point);
            glBufferData(GL_ARRAY_BUFFER, cps.size()*sizeof(vec2), &cps[0], GL_DYNAMIC_DRAW);
            gpuProgram.setUniform(vec3(1,0,0),  "color");
            glDrawArrays(GL_POINTS, 0, cps.size());
        }

        if(cps.size()>=2)
        {
            vector<vec2> vtx;
            for(int i = 0; i < parts; i++)
            {
                float t_normalized = i*1.0f / (parts - 1);
                float t = start() +(end()-start())*t_normalized;
                vec2 tmp = r(t);
                vtx.push_back(tmp);
            }
            glBindVertexArray(vao_curve);
            glBindBuffer(GL_ARRAY_BUFFER, vbo_curve);
            glBufferData(GL_ARRAY_BUFFER, vtx.size()*sizeof(vec2), &vtx[0], GL_DYNAMIC_DRAW);

            gpuProgram.setUniform(vec3(1,1,0),  "color");

            glDrawArrays(GL_LINE_STRIP , 0, parts);

        }

    }

};

Curve *curve;



class BezierCurve : public Curve
{
    float B(int i, float t)
    {
        int n = cps.size()-1;
        float choose = 1;
        for(int j = 1; j <= i; j++)
        {
            choose *= (float)(n-j+1)/j;
        }
        return choose * pow(t, i) * pow(1-t, n-i);

    }
public:
    float start(){return 0;}
    float end(){return 1;}
    virtual vec2 r(float t)
    {
        vec2 rt(0, 0);
        for(int i = 0; i < cps.size(); i++) rt =rt+ cps[i] * B(i,t);
        return rt;
    }
};

class LagrangeCurve : public Curve {
    std::vector<float> knots;  // knots

    float L(int i, float t) {
        float Li = 1.0f;
        for (unsigned int j = 0; j < cps.size(); j++) {
            if (j != i) {
                Li *= (t - knots[j]) / (knots[i] - knots[j]);
            }
        }
        return Li;
    }

public:
    float distance(vec2 p1, vec2 p2)
    {
        float dx = p2.x - p1.x;
        float dy = p2.y - p1.y;
        return sqrt(dx * dx + dy * dy);
    }

    void calculateKnots()
    {
        knots.clear();
        float totalLength = 0.0f;
        for (int i = 1; i < cps.size(); ++i)
        {
            totalLength += distance(cps[i - 1], cps[i]);
        }
        float dist = 0.0f;
        knots.push_back(0.0f); // The first knot is always 0
        for (int i = 1; i < cps.size(); ++i) {
            dist += distance(cps[i - 1], cps[i]);
            knots.push_back(dist/ totalLength);
        }
}

    void AddControlPoint(vec2 p) {

        Curve::AddControlPoint(p);
        calculateKnots();

    }

    float start() override { return 0.0f; }
    float end() override { return knots[cps.size()-1]; }

    vec2 r(float t) override {
        vec2 wPoint = vec2(0, 0);
        for (unsigned int n = 0; n < cps.size(); n++) {
            wPoint = wPoint+ cps[n] * L(n, t);
        }
        return wPoint;
    }
};

class CatmullRom : public Curve
{
private:
    vector<float> knots;
    float tension; // Tension parameter

public:
    CatmullRom() : Curve(), tension(0.0f) {}

    float distance(vec2 p1,  vec2 p2) {
        float dx = p2.x - p1.x;
        float dy = p2.y - p1.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    void calculateKnots() {
        knots.clear();
        float totalLength = 0.0f;
        for (int i = 1; i < cps.size(); ++i) {
            totalLength += distance(cps[i - 1], cps[i]);
        }
        float accumulatedDistance = 0.0f;
        knots.push_back(0.0f); // The first knot is always 0
        for (int i = 1; i < cps.size(); ++i) {
            accumulatedDistance += distance(cps[i - 1], cps[i]);
            knots.push_back(accumulatedDistance / totalLength);
        }
        knots.push_back(1.0f); // The last knot is always 1
    }

   vec2 Hermite(vec2 p0, vec2 v0, float t0, vec2 p1, vec2 v1, float t1, float t) {
       float tdif = t1 - t0;
       vec2 a0 = p0;
       vec2 a1 = v0;
       vec2 a2 = ((3.0f * (p1 - p0)) / (tdif*tdif)) - ((v1 + 2 * v0) / tdif);
       vec2 a3 = ((2.0f * (p0 - p1)) / (tdif*tdif*tdif)) + ((v1 + v0) / (tdif*tdif));
       return (a3 *pow(t-t0,3)) + (a2 * pow(t-t0,2)) + (a1 * (t-t0)) + a0;
        }

    void AddControlPoint(vec2 p) {
        Curve::AddControlPoint(p);
        calculateKnots();
    }
        void SetTension(float initialTension) override {
        tension += initialTension;
    }

    vec2 r(float t) override {
        for (int i = 0; i < cps.size() - 1; i++) {
            if (knots[i] <= t && t <= knots[i + 1]) {
                vec2 tmp = vec2(0,0);
                vec2 q0 = cps[i + 1] - cps[i];
                vec2 q1 = cps[i + 2] - cps[i + 1];
                vec2 q2 = cps[i] - cps[i - 1];
                float t0 = knots[i + 1] - knots[i];
                float t1 = knots[i + 2] - knots[i + 1];
                vec2 v0,v1;
                if(i != 0 && i != (cps.size() - 2))
                {
                    v0 = (q0 / t0 + q2 / (knots[i] - knots[i - 1])) *((1.0f - tension) / 2.0f);
                    v1 = (q1 / t1 + q0 / t0) * ((1.0f - tension) / 2.0f);

                    return Hermite(cps[i], v0, knots[i], cps[i + 1], v1, knots[i + 1], t);
                }
                else  if (i == 0) {
                    v0 = (q0 / t0 + tmp) * ((1.0f - tension) / 2.0f);
                    v1 = (q1 / t1  + q0 / t0) * ((1.0f - tension) / 2.0f);

                    return Hermite(cps[i], v0, knots[i], cps[i + 1], v1, knots[i + 1], t);
                }

                else
                {
                    v0 = (q0 / t0 + q2 / (knots[i] - knots[i - 1])) * ((1.0f - tension) / 2.0f);
                    v1 = (tmp + q0/ t0) * ((1.0f - tension) / 2.0f);

                    return Hermite(cps[i], v0, knots[i], cps[i + 1], v1, knots[i + 1], t);
                }
            }
        }
    }


    float start() override { return knots[0]; }
    float end() override { return knots[cps.size()-1]; }
};


void onInitialization()
{
    glViewport(0, 0, windowWidth, windowHeight);
    curve = new BezierCurve();
    glLineWidth(2.0f);
    glPointSize(10.f);
    gpuProgram.create(vertexSource, fragmentSource, "outColor");
}

// Window has become invalid: Redraw

void onDisplay()
{
    glClearColor(0, 0, 0, 0);     // background color
    glClear(GL_COLOR_BUFFER_BIT); // clear frame buffer
    curve->Draw();
    glutSwapBuffers(); // exchange buffers for double buffering

}

int clicked_point_index = -1;

// Key of ASCII code pressed

void onKeyboard(unsigned char key, int pX, int pY) {
    switch (key) {
        case 'Z':
        {
            camera.Zoom(1.1); //1.1
            glutPostRedisplay();
            printf("zoom in\n");
            break;
        }
        case 'z':
        {
            camera.Zoom(1/1.1); //1/1.1
            glutPostRedisplay();
            printf("zoom out\n");
            break;
        }
        case 'P':
        {
            camera.Pan(vec2(1,0));
            glutPostRedisplay();
            printf("pan left\n");
            break;
        }
        case 'p':
        {
            camera.Pan(vec2(-1,0));
            glutPostRedisplay();
            printf("pan right\n");
            break;
        }
        case 'l':
        {
            delete curve;
            curve = new LagrangeCurve();
            glutPostRedisplay();
            printf("lagrange\n");
            break;
        }
        case 'b':
        {
            delete curve;
            curve = new BezierCurve();
            glutPostRedisplay();
            printf("bezier\n");
            break;
        }
        case 'c':
        {
            delete curve;
            curve = new CatmullRom();
            glutPostRedisplay();
            printf("catmull\n");
            break;
        }
        case 'T':
        {
            curve->SetTension(0.1);
            glutPostRedisplay();
            printf("tension up\n");
            break;
        }
        case 't':
        {
            curve->SetTension(-0.1);
            glutPostRedisplay();
            printf("tension down\n");
            break;
        }
    }

}



// Key of ASCII code released

void onKeyboardUp(unsigned char key, int pX, int pY) {
}



// Move mouse with key pressed

void onMouseMotion(int pX, int pY) {    // pX, pY are the pixel coordinates of the cursor in the coordinate system of the operation system
    // Convert to normalized device space
    float cX = 2.0f * pX / windowWidth - 1;    // flip y axis
    float cY = 1.0f - 2.0f * pY / windowHeight;
    if(clicked_point_index != -1)
    {
        curve->movePoint(clicked_point_index, vec2(cX,cY));
        glutPostRedisplay();
    }
}

// Mouse click event

void onMouse(int button, int state, int pX, int pY) { // pX, pY are the pixel coordinates of the cursor in the coordinate system of the operation system
    // Convert to normalized device space
    float cX = 2.0f * pX / windowWidth - 1;    // flip y axis
    float cY = 1.0f - 2.0f * pY / windowHeight;
    if(button == GLUT_LEFT_BUTTON && state == GLUT_DOWN)
    {
        curve->AddControlPoint(vec2(cX,cY));
        glutPostRedisplay();
    }
    if(button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN)
    {
        clicked_point_index = curve->findPoint(vec2(cX,cY));
        glutPostRedisplay();
    }
    if(button == GLUT_RIGHT_BUTTON && state == GLUT_UP)
    {
        clicked_point_index =-1;
        glutPostRedisplay();
    }

}

// Idle event indicating that some time elapsed: do animation here

void onIdle() {
    long time = glutGet(GLUT_ELAPSED_TIME); // elapsed time since the start of the program
}