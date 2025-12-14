#pragma once
#include "Scene.h"

// ★変更：Renderer の前方宣言（依存を増やしすぎない）
class Renderer;

//ゲームに登場するシーン
enum SCENE_ID
{
    SCENE_ID_TEST = 0,
};

//-----------------------------------------------------------
// SceneManager : シーン切り替え管理（Transform不要）
//
// 変更点(2025-12-12):
// - GameObject 継承を廃止し、Scene を直接管理するクラスに変更
// - Scene は GameObject ではない（Transformを持たない）
//-----------------------------------------------------------
class SceneManager
{
public:
    SceneManager();
    ~SceneManager();

    void Initialize();
    void Update();

    // 既存の即時描画経路（維持）
    void Draw();

    // ★変更：段階移行用（Submit → Execute 方式へ移行するための入口）
    //  この段階では中身は Draw() と同じ処理を行う（動作維持）
    void SubmitDraw(Renderer& renderer);

    void Release();

    // シーン切り替え（次の Update() 冒頭で反映）
    void ChangeScene(SCENE_ID next);

private:
    Scene* CreateScene(SCENE_ID id);
    void DestroyScene(Scene*& scene);

private:
    SCENE_ID currentSceneID_;
    SCENE_ID nextSceneID_;
    Scene* currentScene_;
};
