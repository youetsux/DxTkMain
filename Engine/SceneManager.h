#pragma once
#include "Scene.h"


class Renderer;


enum SCENE_ID
{
    SCENE_ID_TEST = 0,
};

//-----------------------------------------------------------

//



//-----------------------------------------------------------
class SceneManager
{
public:
    SceneManager();
    ~SceneManager();

    void Initialize();
    void Update();


    void Draw();



    void SubmitDraw(Renderer& renderer);

    void Release();


    void ChangeScene(SCENE_ID next);

private:
    Scene* CreateScene(SCENE_ID id);
    void DestroyScene(Scene*& scene);

private:
    SCENE_ID currentSceneID_;
    SCENE_ID nextSceneID_;
    Scene* currentScene_;
};
