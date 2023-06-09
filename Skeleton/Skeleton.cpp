//=============================================================================================
//
// A beadott program csak ebben a fajlban lehet, a fajl 1 byte-os ASCII karaktereket tartalmazhat, BOM kihuzando.
// Tilos:
// - mast "beincludolni", illetve mas konyvtarat hasznalni
// - faljmuveleteket vegezni a printf-et kiveve
// - Mashonnan atvett programresszleteket forrasmegjeloles nelkul felhasznalni es
// - felesleges programsorokat a beadott programban hagyni!!!!!!! 
// - felesleges kommenteket a beadott programba irni a forrasmegjelolest kommentjeit kiveve
// ---------------------------------------------------------------------------------------------
// A feladatot ANSI C++ nyelvu forditoprogrammal ellenorizzuk, a Visual Studio-hoz kepesti elteresekrol
// es a leggyakoribb hibakrol (pl. ideiglenes objektumot nem lehet referencia tipusnak ertekul adni)
// a hazibeado portal ad egy osszefoglalot.
// ---------------------------------------------------------------------------------------------
// A feladatmegoldasokban csak olyan OpenGL fuggvenyek hasznalhatok, amelyek az oran a feladatkiadasig elhangzottak 
// A keretben nem szereplo GLUT fuggvenyek tiltottak.
//
// NYILATKOZAT
// ---------------------------------------------------------------------------------------------
// Nev    : Demeter Abel Bence
// Neptun : HHUZ6K
// ---------------------------------------------------------------------------------------------
// ezennel kijelentem, hogy a feladatot magam keszitettem, es ha barmilyen segitseget igenybe vettem vagy
// mas szellemi termeket felhasznaltam, akkor a forrast es az atvett reszt kommentekben egyertelmuen jeloltem.
// A forrasmegjeloles kotelme vonatkozik az eloadas foliakat es a targy oktatoi, illetve a
// grafhazi doktor tanacsait kiveve barmilyen csatornan (szoban, irasban, Interneten, stb.) erkezo minden egyeb
// informaciora (keplet, program, algoritmus, stb.). Kijelentem, hogy a forrasmegjelolessel atvett reszeket is ertem,
// azok helyessegere matematikai bizonyitast tudok adni. Tisztaban vagyok azzal, hogy az atvett reszek nem szamitanak
// a sajat kontribucioba, igy a feladat elfogadasarol a tobbi resz mennyisege es minosege alapjan szuletik dontes.
// Tudomasul veszem, hogy a forrasmegjeloles kotelmenek megsertese eseten a hazifeladatra adhato pontokat
// negativ elojellel szamoljak el es ezzel parhuzamosan eljaras is indul velem szemben.
//=============================================================================================

#include "framework.h"

const int tessellationLevel = 50;

struct Camera { 
	vec3 wEye, wLookat, wVup;   
	float fov, asp, fp, bp;	
public:
	Camera() {
		asp = (float)windowWidth / windowHeight;
		fov = 75.0f * (float)M_PI / 180.0f;
		fp = 1; bp = 20;
	}
	mat4 V() {
		vec3 w = normalize(wEye - wLookat);
		vec3 u = normalize(cross(wVup, w));
		vec3 v = cross(w, u);
		return TranslateMatrix(wEye * (-1)) * mat4(u.x, v.x, w.x, 0,
			u.y, v.y, w.y, 0,
			u.z, v.z, w.z, 0,
			0, 0, 0, 1);
	}

	mat4 P() { 
		return mat4(1 / (tan(fov / 2) * asp), 0, 0, 0,
			0, 1 / tan(fov / 2), 0, 0,
			0, 0, -(fp + bp) / (bp - fp), -1,
			0, 0, -2 * fp * bp / (bp - fp), 0);
	}
};

struct Material {
	vec3 kd, ks, ka;
	float shininess;
};

struct Light {
	vec3 La, Le;
	vec4 wLightPos;
};

struct RenderState {
	mat4	           MVP, M, Minv, V, P;
	Material* material;
	std::vector<Light> lights;
	vec3	           wEye;
};

class Shader : public GPUProgram {
public:
	virtual void Bind(RenderState state) = 0;

	void setUniformMaterial(const Material& material, const std::string& name) {
		setUniform(material.kd, name + ".kd");
		setUniform(material.ks, name + ".ks");
		setUniform(material.ka, name + ".ka");
		setUniform(material.shininess, name + ".shininess");
	}

	void setUniformLight(const Light& light, const std::string& name) {
		setUniform(light.La, name + ".La");
		setUniform(light.Le, name + ".Le");
		setUniform(light.wLightPos, name + ".wLightPos");
	}
};

class GouraudShader : public Shader {
	const char* vertexSource = R"(
		#version 330
		precision highp float;

		struct Light {
			vec3 La, Le;
			vec4 wLightPos;
		};
		
		struct Material {
			vec3 kd, ks, ka;
			float shininess;
		};

		uniform mat4  MVP, M, Minv;  // MVP, Model, Model-inverse
		uniform Light[8] lights;     // light source direction 
		uniform int   nLights;		 // number of light sources
		uniform vec3  wEye;          // pos of eye
		uniform Material  material;  // diffuse, specular, ambient ref

		layout(location = 0) in vec3  vtxPos;            // pos in modeling space
		layout(location = 1) in vec3  vtxNorm;      	 // normal in modeling space

		out vec3 radiance;		    // reflected radiance

		void main() {
			gl_Position = vec4(vtxPos, 1) * MVP; // to NDC
			// radiance computation
			vec4 wPos = vec4(vtxPos, 1) * M;	
			vec3 V = normalize(wEye * wPos.w - wPos.xyz);
			vec3 N = normalize((Minv * vec4(vtxNorm, 0)).xyz);
			if (dot(N, V) < 0) N = -N;	// prepare for one-sided surfaces like Mobius or Klein

			radiance = vec3(0, 0, 0);
			for(int i = 0; i < nLights; i++) {
				vec3 L = normalize(lights[i].wLightPos.xyz * wPos.w - wPos.xyz * lights[i].wLightPos.w);
				vec3 H = normalize(L + V);
				float cost = max(dot(N,L), 0), cosd = max(dot(N,H), 0);
				radiance += material.ka * lights[i].La + (material.kd * cost + material.ks * pow(cosd, material.shininess)) * lights[i].Le;
			}
		}
	)";

	// fragment shader in GLSL
	const char* fragmentSource = R"(
		#version 330
		precision highp float;

		in  vec3 radiance;      // interpolated radiance
		out vec4 fragmentColor; // output goes to frame buffer

		void main() {
			fragmentColor = vec4(radiance, 1);
		}
	)";
public:
	GouraudShader() { create(vertexSource, fragmentSource, "fragmentColor"); }

	void Bind(RenderState state) {
		Use(); 		// make this program run
		setUniform(state.MVP, "MVP");
		setUniform(state.M, "M");
		setUniform(state.Minv, "Minv");
		setUniform(state.wEye, "wEye");
		setUniformMaterial(*state.material, "material");

		setUniform((int)state.lights.size(), "nLights");
		for (unsigned int i = 0; i < state.lights.size(); i++) {
			setUniformLight(state.lights[i], std::string("lights[") + std::to_string(i) + std::string("]"));
		}
	}
};

class Geometry {
protected:
	unsigned int vao, vbo;      
public:
	Geometry() {
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		glGenBuffers(1, &vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
	}
	virtual void Draw() = 0;
	~Geometry() {
		glDeleteBuffers(1, &vbo);
		glDeleteVertexArrays(1, &vao);
	}
};

class ParamSurface : public Geometry {
	struct VertexData {
		vec3 position, normal;
		vec2 texcoord;
	};

	unsigned int nVtxPerStrip, nStrips;
public:
	ParamSurface() { nVtxPerStrip = nStrips = 0; }

	virtual void eval(float u, float v, vec3& pos, vec3& norm) = 0;

	VertexData GenVertexData(float u, float v) {
		VertexData vtxData;
		vtxData.texcoord = vec2(u, v);
		eval(u, v, vtxData.position, vtxData.normal);

		return vtxData;
	}

	void create(int N = tessellationLevel, int M = tessellationLevel) {
		nVtxPerStrip = (M + 1) * 2;
		nStrips = N;
		std::vector<VertexData> vtxData;	// vertices on the CPU
		for (int i = 0; i < N; i++) {
			for (int j = 0; j <= M; j++) {
				vtxData.push_back(GenVertexData((float)j / M, (float)i / N));
				vtxData.push_back(GenVertexData((float)j / M, (float)(i + 1) / N));
			}
		}
		glBufferData(GL_ARRAY_BUFFER, nVtxPerStrip * nStrips * sizeof(VertexData), &vtxData[0], GL_STATIC_DRAW);
		// Enable the vertex attribute arrays
		glEnableVertexAttribArray(0);  // attribute array 0 = POSITION
		glEnableVertexAttribArray(1);  // attribute array 1 = NORMAL
		glEnableVertexAttribArray(2);  // attribute array 2 = TEXCOORD0
		// attribute array, components/attribute, component type, normalize?, stride, offset
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData, position));
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData, normal));
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData, texcoord));
	}

	void Draw() {
		glBindVertexArray(vao);
		for (unsigned int i = 0; i < nStrips; i++) glDrawArrays(GL_TRIANGLE_STRIP, i * nVtxPerStrip, nVtxPerStrip);
	}
};

class Wall : public ParamSurface{
	vec3 position;
	vec3 dir1;
	vec3 dir2;
	float length;

public:
	Wall(vec3 _position, vec3 _dir1, vec3 _dir2, float _length) {
		position = _position;
		dir1 = _dir1;
		dir2 = _dir2;
		length = _length;

		create();
	}

	void eval(float u, float v, vec3& pos, vec3& norm) override {

		if(length < 0)
			pos = position + u * dir1 + v * dir2;
		else 
			pos = position + u * dir1 + length * v * dir2;

		norm = cross(dir1, dir2);
		norm = normalize(norm);
	}
};

class Terrain : public ParamSurface {
	const float A0 = 1.0f;
	const float fi = 0.0f;
	const unsigned int n = 8;

private:
	float A(float f1, float f2) {
		if (f1 + f2 > 0) {
			return A0 / sqrtf(f1 * f1 + f2 * f2);
		}
		return A0;
	}

public:
	Terrain() { create(); }

	void eval(float u, float v, vec3& pos, vec3& norm) override {
		float x = u * (float)M_PI;
		float y = v * (float)M_PI;

		float h = 0;
		for(int f1 = 0; f1 < n; ++f1)
			for (int f2 = 0; f2 < n; ++f2) {
				h += A(f1, f2) * cosf(f1 * x + f2 * y + fi);
			}
		
		float dx = 0;
		for (int f1 = 0; f1 < n; ++f1)
			for (int f2 = 0; f2 < n; ++f2) {
				dx += -A(f1, f2) * f1 * sinf(f1 * x + f2 * y + fi);
			}

		float dy = 0;
		for (int f1 = 0; f1 < n; ++f1)
			for (int f2 = 0; f2 < n; ++f2) {
				dy += -A(f1, f2) * f2 * sinf(f1 * x + f2 * y + fi);
			}

		if (h > 3.6f)
			h = 3.6f;

		pos = vec3(x, y, h);
		norm = vec3(-dx, -dy, 1);
		norm = normalize(norm);
	}
};

struct Object {
	Shader* shader;
	Material* material;
	Geometry* geometry;
	vec3 scale, translation, rotationAxis;
	float rotationAngle;
public:
	Object(Shader* _shader, Material* _material, Geometry* _geometry) :
		scale(vec3(1, 1, 1)), translation(vec3(0, 0, 0)), rotationAxis(0, 0, 1), rotationAngle(0) {
		shader = _shader;
		material = _material;
		geometry = _geometry;
	}

	virtual void SetModelingTransform(mat4& M, mat4& Minv) {
		M = ScaleMatrix(scale) * RotationMatrix(rotationAngle, rotationAxis) * TranslateMatrix(translation);
		Minv = TranslateMatrix(-translation) * RotationMatrix(-rotationAngle, rotationAxis) * ScaleMatrix(vec3(1 / scale.x, 1 / scale.y, 1 / scale.z));
	}

	void Draw(RenderState state) {
		mat4 M, Minv;
		SetModelingTransform(M, Minv);
		state.M = M;
		state.Minv = Minv;
		state.MVP = state.M * state.V * state.P;
		state.material = material;
		shader->Bind(state);
		geometry->Draw();
	}
};

class Scene {
	std::vector<Object*> objects;
	Camera camera;
	std::vector<Light> lights;
public:
	void Build() {
		// Shaders
		Shader* gouraudShader = new GouraudShader();

		// Materials
		Material* material0 = new Material;
		material0->kd = vec3(0.6f, 0.4f, 0.2f);
		material0->ks = vec3(4, 4, 4);
		material0->ka = vec3(0.1f, 0.1f, 0.1f);
		material0->shininess = 100;

		Material* material1 = new Material;
		material1->kd = vec3(0.5f, 0.0f, 0.0f);
		material1->ks = vec3(0.5f, 0.0f, 0.f);
		material1->ka = vec3(0.2f, 0.2f, 0.2f);
		material1->shininess = 30;

		//Geometries
		Wall* wall1 = new Wall(vec3(0, 0, 0), vec3(1, 0, 0), vec3(0, 1, 0), 2.0f);
		Wall* wall2 = new Wall(vec3(0, 0, 0), vec3(0, 0, 1), vec3(0, 1, 0), 2.0f);
		Wall* wall3 = new Wall(vec3(0, 2, 0), vec3(1, 0, 0), vec3(0, 0, 1), -1.0f);
		Terrain* terrain = new Terrain();
		
		//objects
		Object* terrainObject1 = new Object(gouraudShader, material0, terrain);
		objects.push_back(terrainObject1);
		terrainObject1->rotationAngle = -(float)M_PI / 2;
		terrainObject1->rotationAxis = vec3(1.0f, 0.0f, 0.0f);
		terrainObject1->translation = vec3(-3, -2, 0);
		terrainObject1->scale = vec3(1.0f, 1.0f, 1.0f) * 2.3f;

		vec3 wallScale = vec3(0.6f, 0.6f, 0.6f);

		Object* wallObject1 = new Object(gouraudShader, material1, wall1);
		objects.push_back(wallObject1);
		wallObject1->translation = vec3(0, -1.5f, -5);
		wallObject1->rotationAxis = vec3(0, 1, 0);
		wallObject1->rotationAngle = -(float)M_PI / 6;
		wallObject1->scale = wallScale;

		Object* wallObject2 = new Object(gouraudShader, material1, wall2);
		objects.push_back(wallObject2);
		wallObject2->translation = vec3(0, -1.5f, -5);
		wallObject2->rotationAxis = vec3(0, 1, 0);
		wallObject2->rotationAngle = -(float)M_PI / 6;
		wallObject2->scale = wallScale;

		Object* wallObject3 = new Object(gouraudShader, material1, wall3);
		objects.push_back(wallObject3);
		wallObject3->translation = vec3(0, -1.5f, -5);
		wallObject3->rotationAxis = vec3(0, 1, 0);
		wallObject3->rotationAngle = -(float)M_PI / 6;
		wallObject3->scale = wallScale;

		// Camera
		camera.wEye = vec3(0, 0, -8);
		camera.wLookat = vec3(0, 0, 0);
		camera.wVup = vec3(0, 1, 0);

		// Lights
		lights.resize(2);
		lights[0].wLightPos = vec4(5, 10, -10, 0);
		lights[0].La = vec3(0.1f, 0.1f, 1);
		lights[0].Le = vec3(1.5f, 0, 0);

		lights[1].wLightPos = vec4(5, 10, -20, 0);
		lights[1].La = vec3(0.2f, 0.2f, 0.2f);
		lights[1].Le = vec3(0, 0.9f, 0);
	}

	void Render() {
		RenderState state;
		state.wEye = camera.wEye;
		state.V = camera.V();
		state.P = camera.P();
		state.lights = lights;
		for (Object* obj : objects) obj->Draw(state);
	}
};

Scene scene;

// Initialization, create an OpenGL context
void onInitialization() {
	glViewport(0, 0, windowWidth, windowHeight);
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	scene.Build();
}

// Window has become invalid: Redraw
void onDisplay() {
	glClearColor(0.5f, 0.5f, 0.8f, 1.0f);							// background color 
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // clear the screen
	scene.Render();
	glutSwapBuffers();									// exchange the two buffers
}

// Key of ASCII code pressed
void onKeyboard(unsigned char key, int pX, int pY) { }

// Key of ASCII code released
void onKeyboardUp(unsigned char key, int pX, int pY) { }

// Mouse click event
void onMouse(int button, int state, int pX, int pY) { }

// Move mouse with key pressed
void onMouseMotion(int pX, int pY) {
}

// Idle event indicating that some time elapsed: do animation here
void onIdle() {
	glutPostRedisplay();
}