//***************************************************************************************
// ShapesApp.cpp by Frank Luna (C) 2015 All Rights Reserved.
//
// Hold down '1' key to view scene in wireframe mode.
//***************************************************************************************

#include "../OldCommon/d3dApp.h"
#include "../OldCommon/MathHelper.h"
#include "../OldCommon/UploadBuffer.h"
#include "../OldCommon/GeometryGenerator.h"
#include "FrameResource.h"
#include <string>
#include <map>
using namespace std;

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

const int gNumFrameResources = 3;
static float GRAN = 4;              // Границы игрового поля (верх и низ)
static float FieldWidth = 15.0f;                    // Игровое поле: ширина
static float FieldHeight = 10.0f;                   // Игровое поле: высота
static float BaseBallSpeed = 7.0f;                  // Игровое поле: базовая скорость мяча
static float PlayerPosition = 6.0f;                 // Игрок: стартовое смещение по горизонтали
static float PlayerSpeed = 7.5f;                    // Игрок: скорость платформы
static float PlayerHeight = 2.0f;                   // Игрок: ширина платформы
static float LineThickness = 0.2f;                  // Графика: толщина линий
static float DeltaForceOffset = 2.0f;               // Дельта изменения отскока
static DirectX::XMVECTORF32 colplayer1 = DirectX::Colors::DarkRed;
static DirectX::XMVECTORF32 colplayer2 = DirectX::Colors::DarkRed;
static DirectX::XMVECTORF32 colwall = DirectX::Colors::ForestGreen;
static DirectX::XMVECTORF32 colball = DirectX::Colors::Purple;

// Предрасчёты
static float HalfPlayerHeight = PlayerHeight / 2;
static float HalfLineThickness = LineThickness / 2;
static float CLAMP_ARG = FieldHeight / 2 - PlayerHeight / 2;

// Описание объекта:
//  - Позиция
//  - Размеры
struct GameObject
{
    struct Position
    {
        float X, Y, Z;
    } Position;
    struct Scale
    {
        float X, Y, Z;
    } Scale;
    std::string color;
};

void clamp(float& val, float min, float max) {
    if (val < min)
        val = min;
    if (val > max)
        val = max;
}

float frand(float a, float b) {
    return a + b * static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
}

void normalize(float * arr)
{
    float length = sqrt(arr[0] * arr[0] + arr[1] * arr[1]);
    arr[0] = arr[0] / length;
    arr[1] = arr[1] / length;
}

void forcefunc(float* force, float relativeoffset)
{
    // Инверсия горизонтальной скорости
    force[0] = -force[0];

    // Инверсия вертикальной скорости
    force[1] = force[1] + frand(-0.33f, 0.33f);// relativeoffset* DeltaForceOffset;
    //clamp(force[1], -force[0] * 0.5f, force[0] * 0.5f);
    
    normalize(force);
}

// Lightweight structure stores parameters to draw a shape.  This will
// vary from app-to-app.
struct RenderItem
{
    RenderItem() = default;

    // World matrix of the shape that describes the object's local space
    // relative to the world space, which defines the position, orientation,
    // and scale of the object in the world.
    XMFLOAT4X4 World = MathHelper::Identity4x4();

    // Dirty flag indicating the object data has changed and we need to update the constant buffer.
    // Because we have an object cbuffer for each FrameResource, we have to apply the
    // update to each FrameResource.  Thus, when we modify obect data we should set 
    // NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
    int NumFramesDirty = gNumFrameResources;

    // Index into GPU constant buffer corresponding to the ObjectCB for this render item.
    UINT ObjCBIndex = -1;

    MeshGeometry* Geo = nullptr;

    // Primitive topology.
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    // DrawIndexedInstanced parameters.
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    int BaseVertexLocation = 0;
};

class ShapesApp : public D3DApp
{
public:
    ShapesApp(HINSTANCE hInstance);
    ShapesApp(const ShapesApp& rhs) = delete;
    ShapesApp& operator=(const ShapesApp& rhs) = delete;
    ~ShapesApp();

    virtual bool Initialize()override;

private:
    virtual void OnResize()override;
    virtual void Update(const GameTimer& gt)override;
    virtual void Draw(const GameTimer& gt)override;

    virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
    virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y)override;

    void OnKeyboardInput(const GameTimer& gt);
    void UpdateCamera(const GameTimer& gt);
    void UpdateObjectCBs(const GameTimer& gt);
    void UpdateMainPassCB(const GameTimer& gt);
    void UpdateRender(const GameTimer& gt);
    void UpdateGameObjects(const GameTimer& gt);
    void CheckCollision();
    void ResetObjects(bool);

    void BuildDescriptorHeaps();
    void BuildConstantBufferViews();
    void BuildRootSignature();
    void BuildShadersAndInputLayout();
    void BuildShapeGeometry();
    void BuildPSOs();
    void BuildFrameResources();
    void BuildRenderItems();
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

private:

    std::vector<std::unique_ptr<FrameResource>> mFrameResources;
    FrameResource* mCurrFrameResource = nullptr;
    int mCurrFrameResourceIndex = 0;

    ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
    ComPtr<ID3D12DescriptorHeap> mCbvHeap = nullptr;

    ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

    std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
    std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
    std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

    // List of all the render items.
    std::map<std::string, std::unique_ptr<RenderItem>> mAllRitems;

    // Render items divided by PSO.
    std::vector<RenderItem*> mOpaqueRitems;

    PassConstants mMainPassCB;

    UINT mPassCbvOffset = 0;

    bool mIsWireframe = true;

    XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };
    XMFLOAT4X4 mView = MathHelper::Identity4x4();
    XMFLOAT4X4 mProj = MathHelper::Identity4x4();

    float mTheta = 1.5f * XM_PI;
    float mPhi = 0.5f * XM_PI;
    float mRadius = 15.0f;

    POINT mLastMousePos;

    // Игровые переменные
    std::map<string, GameObject> go;
    bool IsGameRunning = false;
    int ScoreP1 = 0;
    int ScoreP2 = 0;
    float force[2];
    float BallSpeed;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
    PSTR cmdLine, int showCmd)
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    try
    {
        ShapesApp theApp(hInstance);
        if (!theApp.Initialize())
            return 0;

        return theApp.Run();
    }
    catch (DxException& e)
    {
        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        return 0;
    }
}

// Инициализация игровых объектов (установка базовых параметров)
ShapesApp::ShapesApp(HINSTANCE hInstance)
    : D3DApp(hInstance)
{
#pragma region Ball
    GameObject go_ball;
    go_ball.Position.X = 0.0f;
    go_ball.Position.Y = 0.0f;
    go_ball.Position.Z = 0.0f;
    go_ball.Scale.X = LineThickness;
    go_ball.Scale.Y = LineThickness;
    go_ball.Scale.Z = 0.0f;
    go_ball.color = "ball";
    go["ball"] = go_ball;
#pragma endregion

#pragma region Walls
    GameObject go_upwall;
    go_upwall.Position.X = 0.0f;
    go_upwall.Position.Y = FieldHeight / 2 + LineThickness / 2;
    go_upwall.Position.Z = 0.0f;
    go_upwall.Scale.X = FieldWidth;
    go_upwall.Scale.Y = LineThickness;
    go_upwall.Scale.Z = 0.0f;
    go_upwall.color = "wall";
    go["upwall"] = go_upwall;

    GameObject go_downwall;
    go_downwall.Position.X = 0.0f;
    go_downwall.Position.Y = -(FieldHeight / 2) - LineThickness / 2;
    go_downwall.Position.Z = 0.0f;
    go_downwall.Scale.X = FieldWidth;
    go_downwall.Scale.Y = LineThickness;
    go_downwall.Scale.Z = 0.0f;
    go_downwall.color = "wall";
    go["downwall"] = go_downwall;

    GameObject go_leftwall;
    go_leftwall.Position.X = -(FieldWidth / 2) - LineThickness / 2;
    go_leftwall.Position.Y = 0.0f;
    go_leftwall.Position.Z = 0.0f;
    go_leftwall.Scale.X = LineThickness;
    go_leftwall.Scale.Y = FieldHeight + 2 * LineThickness;
    go_leftwall.Scale.Z = 0.0f;
    go_leftwall.color = "wall";
    go["leftwall"] = go_leftwall;

    GameObject go_rightwall;
    go_rightwall.Position.X = FieldWidth / 2 + LineThickness / 2;
    go_rightwall.Position.Y = 0.0f;
    go_rightwall.Position.Z = 0.0f;
    go_rightwall.Scale.X = LineThickness;
    go_rightwall.Scale.Y = FieldHeight + 2 * LineThickness;
    go_rightwall.Scale.Z = 0.0f;
    go_rightwall.color = "wall";
    go["rightwall"] = go_rightwall;
#pragma endregion

#pragma region Players
    GameObject go_palyer1;
    go_palyer1.Position.X = -PlayerPosition;
    go_palyer1.Position.Y = 0.0f;
    go_palyer1.Position.Z = 0.0f;
    go_palyer1.Scale.X = LineThickness;
    go_palyer1.Scale.Y = PlayerHeight;
    go_palyer1.Scale.Z = 0.0f;
    go_palyer1.color = "player1";
    go["player1"] = go_palyer1;

    GameObject go_palyer2;
    go_palyer2.Position.X = PlayerPosition;
    go_palyer2.Position.Y = 0.0f;
    go_palyer2.Position.Z = 0.0f;
    go_palyer2.Scale.X = LineThickness;
    go_palyer2.Scale.Y = PlayerHeight;
    go_palyer2.Scale.Z = 0.0f;
    go_palyer2.color = "player2";
    go["player2"] = go_palyer2;
#pragma endregion
}

ShapesApp::~ShapesApp()
{
    if (md3dDevice != nullptr)
        FlushCommandQueue();
}

bool ShapesApp::Initialize()
{
    if (!D3DApp::Initialize())
        return false;

    // Reset the command list to prep for initialization commands.
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    BuildRootSignature();
    BuildShadersAndInputLayout();
    BuildShapeGeometry();
    BuildRenderItems();
    BuildFrameResources();
    BuildDescriptorHeaps();
    BuildConstantBufferViews();
    BuildPSOs();

    // Execute the initialization commands.
    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Wait until initialization is complete.
    FlushCommandQueue();

    return true;
}

void ShapesApp::OnResize()
{
    D3DApp::OnResize();

    // The window resized, so update the aspect ratio and recompute the projection matrix.
    XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
    XMStoreFloat4x4(&mProj, P);
}

void ShapesApp::Update(const GameTimer& gt)
{
    OnKeyboardInput(gt);
    UpdateCamera(gt);

    // Цикл игровых объектов

    UpdateGameObjects(gt);
    CheckCollision();

    // Цикл рендера

    UpdateRender(gt);

    // Циклический обход по массиву ресурсов кругового кадра
    mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
    mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

    // Ожидание завершения работы GPU до текущей точки ограждения
    if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
        ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }

    UpdateObjectCBs(gt);
    UpdateMainPassCB(gt);
}

void ShapesApp::UpdateGameObjects(const GameTimer& gt)
{
    // Первичная скорость шара
    if (IsGameRunning)
    {
        if (force[0] == 0.0f && force[1] == 0.0f) {
            int r1 = rand() % 2 ? -1 : 1;
            int r2 = rand() % 2 ? -1 : 1;
            force[0] = r1 * frand(3.0f, 6.0f);
            force[1] = r2 * frand(0.0f, 6.0f);
            BallSpeed = BaseBallSpeed;
            normalize(force);
        }
    }

    // Обновляем позицию шара
    go["ball"].Position.X = go["ball"].Position.X + force[0] * BallSpeed * gt.DeltaTime();
    go["ball"].Position.Y = go["ball"].Position.Y + force[1] * BallSpeed * gt.DeltaTime();

    // Проверка колизии с верхней стеной
    if (go["ball"].Position.Y >= FieldHeight / 2 - LineThickness / 2)
    {
        // Вычисляем угол поворота
        force[1] = -force[1];
    }

    // Проверка колизии с нижней стеной
    if (go["ball"].Position.Y <= -(FieldHeight / 2) + LineThickness / 2)
    {
        // Вычисляем угол поворота
        force[1] = -force[1];
    }

    // Проверка колизии с левым игроком
    if (go["ball"].Position.X <= -PlayerPosition + LineThickness)
    {
        float waterline = go["ball"].Position.Y - go["player1"].Position.Y;
        float forceoff = waterline / HalfPlayerHeight + HalfLineThickness;

        // Верхняя часть платформы (строго выше середины)
        if (HalfPlayerHeight + HalfLineThickness >= waterline && waterline > 0)
        {
            forcefunc(force, forceoff);
            BallSpeed = BaseBallSpeed + frand(1.0f, 3.0f);
            go["ball"].Position.X = -PlayerPosition + LineThickness;
        }

        // Нижняя часть платформы (ниже или равен середине)
        if (0 >= waterline && waterline >= -HalfPlayerHeight - HalfLineThickness)
        {
            forcefunc(force, forceoff);
            BallSpeed = BaseBallSpeed + frand(1.0f, 3.0f);
            go["ball"].Position.X = -PlayerPosition + LineThickness;
        }
    }

    // Проверка колизии с правым игроком
    if (go["ball"].Position.X >= PlayerPosition - LineThickness)
    {
        float waterline = go["ball"].Position.Y - go["player2"].Position.Y;
        float forceoff = waterline / (HalfPlayerHeight + HalfLineThickness);

        // Верхняя часть платформы (строго выше середины)
        if (HalfPlayerHeight + HalfLineThickness >= waterline && waterline > 0)
        {
            forcefunc(force, forceoff);
            BallSpeed = BaseBallSpeed + frand(1.0f, 3.0f);
            go["ball"].Position.X = PlayerPosition - LineThickness;
        }

        // Нижняя часть платформы (ниже или равен середине)
        if (0 >= waterline && waterline >= -HalfPlayerHeight - HalfLineThickness)
        {
            forcefunc(force, forceoff);
            BallSpeed = BaseBallSpeed + frand(1.0f, 3.0f);
            go["ball"].Position.X = PlayerPosition - LineThickness;
        }
    }
}

void ShapesApp::CheckCollision()
{
    // Даём очко первому игроку
    if (go["ball"].Position.X < go["player1"].Position.X)
    {
        ScoreP1++;
        ResetObjects(false);
    }

    // Даём очко второму игроку
    if (go["ball"].Position.X > go["player2"].Position.X)
    {
        ScoreP2++;
        ResetObjects(false);
    }
}

void ShapesApp::ResetObjects(bool IsNewGame = false)
{
    IsGameRunning = false;
    BallSpeed = 0.0f;
    go["player1"].Position.Y = 0.0f;
    go["player2"].Position.Y = 0.0f;
    go["ball"].Position.X = 0.0f;
    go["ball"].Position.Y = 0.0f;
    force[0] = 0.0f;
    force[1] = 0.0f;

    if (IsNewGame) {
        ScoreP1 = 0;
        ScoreP2 = 0;
    }
}

// Обработка "физики" объектов
void ShapesApp::UpdateRender(const GameTimer& gt)
{
    for (auto& it : mAllRitems)
    {
        GameObject& object = go[it.first];
        XMStoreFloat4x4(&it.second->World, 
            XMMatrixScaling(
                object.Scale.X,
                object.Scale.Y,
                object.Scale.Z) *
            XMMatrixTranslation(
                object.Position.X,
                object.Position.Y,
                object.Position.Z));
    }
}

void ShapesApp::Draw(const GameTimer& gt)
{
    auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

    // Reuse the memory associated with command recording.
    // We can only reset when the associated command lists have finished execution on the GPU.
    ThrowIfFailed(cmdListAlloc->Reset());

    // A command list can be reset after it has been added to the command queue via ExecuteCommandList.
    // Reusing the command list reuses memory.
    if (mIsWireframe)
    {
        ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque_wireframe"].Get()));
    }
    else
    {
        ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));
    }

    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    // Indicate a state transition on the resource usage.
    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    // Clear the back buffer and depth buffer.
    mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
    mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    // Specify the buffers we are going to render to.
    mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

    ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvHeap.Get() };
    mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

    int passCbvIndex = mPassCbvOffset + mCurrFrameResourceIndex;
    auto passCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
    passCbvHandle.Offset(passCbvIndex, mCbvSrvUavDescriptorSize);
    mCommandList->SetGraphicsRootDescriptorTable(1, passCbvHandle);

    DrawRenderItems(mCommandList.Get(), mOpaqueRitems);

    // Indicate a state transition on the resource usage.
    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    // Done recording commands.
    ThrowIfFailed(mCommandList->Close());

    // Add the command list to the queue for execution.
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Swap the back and front buffers
    ThrowIfFailed(mSwapChain->Present(0, 0));
    mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

    // Advance the fence value to mark commands up to this fence point.
    mCurrFrameResource->Fence = ++mCurrentFence;

    // Add an instruction to the command queue to set a new fence point. 
    // Because we are on the GPU timeline, the new fence point won't be 
    // set until the GPU finishes processing all the commands prior to this Signal().
    mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void ShapesApp::OnMouseDown(WPARAM btnState, int x, int y)
{
    mLastMousePos.x = x;
    mLastMousePos.y = y;

    SetCapture(mhMainWnd);
}

void ShapesApp::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

void ShapesApp::OnMouseMove(WPARAM btnState, int x, int y)
{
    //if ((btnState & MK_LBUTTON) != 0)
    //{
    //    // Make each pixel correspond to a quarter of a degree.
    //    float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
    //    float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

    //    // Update angles based on input to orbit camera around box.
    //    mTheta += dx;
    //    mPhi += dy;

    //    // Restrict the angle mPhi.
    //    mPhi = MathHelper::Clamp(mPhi, 0.1f, MathHelper::Pi - 0.1f);
    //}
    //else if ((btnState & MK_RBUTTON) != 0)
    //{
    //    // Make each pixel correspond to 0.2 unit in the scene.
    //    float dx = 0.05f * static_cast<float>(x - mLastMousePos.x);
    //    float dy = 0.05f * static_cast<float>(y - mLastMousePos.y);

    //    // Update the camera radius based on input.
    //    mRadius += dx - dy;

    //    // Restrict the radius.
    //    mRadius = MathHelper::Clamp(mRadius, 5.0f, 150.0f);
    //}

    mLastMousePos.x = x;
    mLastMousePos.y = y;
}

// Обработка нажатий на кнопки клавиатуры
void ShapesApp::OnKeyboardInput(const GameTimer& gt)
{
    // Выбор режима отображения полигонов
    mIsWireframe = GetAsyncKeyState('1') & 0x8000;

    // Рестарт
    if (GetAsyncKeyState('Y') & 0x8000)
    {
        ResetObjects(true);
    }

    // Новая игра
    if (GetAsyncKeyState(32) & 0x8000) {
        // Работает только если игра не запущена
        if (!IsGameRunning)
            IsGameRunning = true;
    }

    if (GetAsyncKeyState('Q') & 0x8000)
        go["player1"].Position.Y = go["player1"].Position.Y + PlayerSpeed * gt.DeltaTime();

    if (GetAsyncKeyState('A') & 0x8000)
        go["player1"].Position.Y = go["player1"].Position.Y - PlayerSpeed * gt.DeltaTime();

    if (GetAsyncKeyState('O') & 0x8000)
        go["player2"].Position.Y = go["player2"].Position.Y + PlayerSpeed * gt.DeltaTime();

    if (GetAsyncKeyState('L') & 0x8000)
        go["player2"].Position.Y = go["player2"].Position.Y - PlayerSpeed * gt.DeltaTime();

    clamp(go["player1"].Position.Y, -CLAMP_ARG, CLAMP_ARG);
    clamp(go["player2"].Position.Y, -CLAMP_ARG, CLAMP_ARG);
}

void ShapesApp::UpdateCamera(const GameTimer& gt)
{
    // Convert Spherical to Cartesian coordinates.
    mEyePos.x = mRadius * sinf(mPhi) * cosf(mTheta);
    mEyePos.z = mRadius * sinf(mPhi) * sinf(mTheta);
    mEyePos.y = mRadius * cosf(mPhi);

    // Build the view matrix.
    XMVECTOR pos = XMVectorSet(mEyePos.x, mEyePos.y, mEyePos.z, 1.0f);
    XMVECTOR target = XMVectorZero();
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
    XMStoreFloat4x4(&mView, view);
}

void ShapesApp::UpdateObjectCBs(const GameTimer& gt)
{
    auto currObjectCB = mCurrFrameResource->ObjectCB.get();
    for (auto& e : mAllRitems)
    {
        // Only update the cbuffer data if the constants have changed.  
        // This needs to be tracked per frame resource.
        //if (e->NumFramesDirty > 0)
        {
            XMMATRIX world = XMLoadFloat4x4(&e.second->World);

            ObjectConstants objConstants;
            XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));

            currObjectCB->CopyData(e.second->ObjCBIndex, objConstants);

            // Next FrameResource need to be updated too.
            e.second->NumFramesDirty--;
        }
    }
}

void ShapesApp::UpdateMainPassCB(const GameTimer& gt)
{
    XMMATRIX view = XMLoadFloat4x4(&mView);
    XMMATRIX proj = XMLoadFloat4x4(&mProj);

    XMMATRIX viewProj = XMMatrixMultiply(view, proj);
    XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
    XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
    XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

    XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
    XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
    XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
    XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
    XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
    XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
    mMainPassCB.EyePosW = mEyePos;
    mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
    mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
    mMainPassCB.NearZ = 1.0f;
    mMainPassCB.FarZ = 1000.0f;
    mMainPassCB.TotalTime = gt.TotalTime();
    mMainPassCB.DeltaTime = gt.DeltaTime();

    auto currPassCB = mCurrFrameResource->PassCB.get();
    currPassCB->CopyData(0, mMainPassCB);
}

void ShapesApp::BuildDescriptorHeaps()
{
    UINT objCount = (UINT)mOpaqueRitems.size();

    // Need a CBV descriptor for each object for each frame resource,
    // +1 for the perPass CBV for each frame resource.
    UINT numDescriptors = (objCount + 1) * gNumFrameResources;

    // Save an offset to the start of the pass CBVs.  These are the last 3 descriptors.
    mPassCbvOffset = objCount * gNumFrameResources;

    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
    cbvHeapDesc.NumDescriptors = numDescriptors;
    cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    cbvHeapDesc.NodeMask = 0;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&cbvHeapDesc,
        IID_PPV_ARGS(&mCbvHeap)));
}

void ShapesApp::BuildConstantBufferViews()
{
    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

    UINT objCount = (UINT)mOpaqueRitems.size();

    // Need a CBV descriptor for each object for each frame resource.
    for (int frameIndex = 0; frameIndex < gNumFrameResources; ++frameIndex)
    {
        auto objectCB = mFrameResources[frameIndex]->ObjectCB->Resource();
        for (UINT i = 0; i < objCount; ++i)
        {
            D3D12_GPU_VIRTUAL_ADDRESS cbAddress = objectCB->GetGPUVirtualAddress();

            // Offset to the ith object constant buffer in the buffer.
            cbAddress += i * objCBByteSize;

            // Offset to the object cbv in the descriptor heap.
            int heapIndex = frameIndex * objCount + i;
            auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvHeap->GetCPUDescriptorHandleForHeapStart());
            handle.Offset(heapIndex, mCbvSrvUavDescriptorSize);

            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
            cbvDesc.BufferLocation = cbAddress;
            cbvDesc.SizeInBytes = objCBByteSize;

            md3dDevice->CreateConstantBufferView(&cbvDesc, handle);
        }
    }

    UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants));

    // Last three descriptors are the pass CBVs for each frame resource.
    for (int frameIndex = 0; frameIndex < gNumFrameResources; ++frameIndex)
    {
        auto passCB = mFrameResources[frameIndex]->PassCB->Resource();
        D3D12_GPU_VIRTUAL_ADDRESS cbAddress = passCB->GetGPUVirtualAddress();

        // Offset to the pass cbv in the descriptor heap.
        int heapIndex = mPassCbvOffset + frameIndex;
        auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvHeap->GetCPUDescriptorHandleForHeapStart());
        handle.Offset(heapIndex, mCbvSrvUavDescriptorSize);

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
        cbvDesc.BufferLocation = cbAddress;
        cbvDesc.SizeInBytes = passCBByteSize;

        md3dDevice->CreateConstantBufferView(&cbvDesc, handle);
    }
}

void ShapesApp::BuildRootSignature()
{
    CD3DX12_DESCRIPTOR_RANGE cbvTable0;
    cbvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

    CD3DX12_DESCRIPTOR_RANGE cbvTable1;
    cbvTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);

    // Root parameter can be a table, root descriptor or root constants.
    CD3DX12_ROOT_PARAMETER slotRootParameter[2];

    // Create root CBVs.
    slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable0);
    slotRootParameter[1].InitAsDescriptorTable(1, &cbvTable1);

    // A root signature is an array of root parameters.
    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, slotRootParameter, 0, nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    // create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

    if (errorBlob != nullptr)
    {
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);

    ThrowIfFailed(md3dDevice->CreateRootSignature(
        0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void ShapesApp::BuildShadersAndInputLayout()
{
    mShaders["standardVS"] = d3dUtil::CompileShader(L"Color.hlsl", nullptr, "VS", "vs_5_1");
    mShaders["opaquePS"] = d3dUtil::CompileShader(L"Color.hlsl", nullptr, "PS", "ps_5_1");

    mInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
}

void ShapesApp::BuildShapeGeometry()
{
    // Генерируем геометрию
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData player1 = geoGen.CreateBox(1.0f, 1.0f, 0.0f, 0); // Стандартный цвет
    GeometryGenerator::MeshData player2 = geoGen.CreateBox(1.0f, 1.0f, 0.0f, 0); // Стандартный цвет
    GeometryGenerator::MeshData ball = geoGen.CreateBox(1.0f, 1.0f, 0.0f, 0); // Стандартный цвет
    GeometryGenerator::MeshData wall = geoGen.CreateBox(1.0f, 1.0f, 0.0f, 0); // Стандартный цвет

    //
    // We are concatenating all the geometry into one big vertex/index buffer.  So
    // define the regions in the buffer each submesh covers.
    //

    // Cache the vertex offsets to each object in the concatenated vertex buffer.
    UINT player1VertexOffset = 0;
    UINT player2VertexOffset = (UINT)player1.Vertices.size();
    UINT ballVertexOffset = player2VertexOffset + (UINT)ball.Vertices.size();
    UINT wallVertexOffset = ballVertexOffset + (UINT)wall.Vertices.size();

    // Cache the starting index for each object in the concatenated index buffer.
    UINT player1IndexOffset = 0;
    UINT player2IndexOffset = (UINT)player1.Indices32.size();
    UINT ballIndexOffset = player2IndexOffset + (UINT)ball.Indices32.size();
    UINT wallIndexOffset = ballIndexOffset + (UINT)wall.Indices32.size();

    SubmeshGeometry player1Submesh;
    player1Submesh.IndexCount = (UINT)player1.Indices32.size();
    player1Submesh.StartIndexLocation = player1IndexOffset;
    player1Submesh.BaseVertexLocation = player1VertexOffset;

    SubmeshGeometry player2Submesh;
    player2Submesh.IndexCount = (UINT)player2.Indices32.size();
    player2Submesh.StartIndexLocation = player2IndexOffset;
    player2Submesh.BaseVertexLocation = player2VertexOffset;

    SubmeshGeometry ballSubmesh;
    ballSubmesh.IndexCount = (UINT)ball.Indices32.size();
    ballSubmesh.StartIndexLocation = ballIndexOffset;
    ballSubmesh.BaseVertexLocation = ballVertexOffset;

    SubmeshGeometry wallSubmesh;
    wallSubmesh.IndexCount = (UINT)wall.Indices32.size();
    wallSubmesh.StartIndexLocation = wallIndexOffset;
    wallSubmesh.BaseVertexLocation = wallVertexOffset;

    // Упаковка вершин в единый буфер

    // Считаем общее количество вершин в геометрии
    auto totalVertexCount =
        player1.Vertices.size() +       // Добавляем количество вершин в платформах
        player2.Vertices.size() +       // Добавляем количество вершин в платформах
        ball.Vertices.size() +       // Добавляем количество вершин в платформах
        wall.Vertices.size();  // Добавляем количество вершин в сквадболе

    std::vector<Vertex> vertices(totalVertexCount);

    UINT k = 0;
    for (size_t i = 0; i < player1.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = player1.Vertices[i].Position;
        vertices[k].Color = XMFLOAT4(colplayer1);
    }

    for (size_t i = 0; i < player2.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = player2.Vertices[i].Position;
        vertices[k].Color = XMFLOAT4(colplayer2);
    }

    for (size_t i = 0; i < ball.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = ball.Vertices[i].Position;
        vertices[k].Color = XMFLOAT4(colball);
    }

    for (size_t i = 0; i < wall.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = wall.Vertices[i].Position;
        vertices[k].Color = XMFLOAT4(colwall);
    }

    std::vector<std::uint16_t> indices;
    indices.insert(indices.end(), std::begin(player1.GetIndices16()), std::end(player1.GetIndices16()));
    indices.insert(indices.end(), std::begin(player2.GetIndices16()), std::end(player2.GetIndices16()));
    indices.insert(indices.end(), std::begin(ball.GetIndices16()), std::end(ball.GetIndices16()));
    indices.insert(indices.end(), std::begin(wall.GetIndices16()), std::end(wall.GetIndices16()));

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "shapeGeo";

    ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
    CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
    CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

    geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

    geo->VertexByteStride = sizeof(Vertex);
    geo->VertexBufferByteSize = vbByteSize;
    geo->IndexFormat = DXGI_FORMAT_R16_UINT;
    geo->IndexBufferByteSize = ibByteSize;

    geo->DrawArgs["player1"] = player1Submesh;
    geo->DrawArgs["player2"] = player2Submesh;
    geo->DrawArgs["wall"] = wallSubmesh;
    geo->DrawArgs["ball"] = ballSubmesh;

    mGeometries[geo->Name] = std::move(geo);
}

void ShapesApp::BuildPSOs()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

    //
    // PSO for opaque objects.
    //
    ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    opaquePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
    opaquePsoDesc.pRootSignature = mRootSignature.Get();
    opaquePsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()),
        mShaders["standardVS"]->GetBufferSize()
    };
    opaquePsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
        mShaders["opaquePS"]->GetBufferSize()
    };
    opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    opaquePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    opaquePsoDesc.SampleMask = UINT_MAX;
    opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    opaquePsoDesc.NumRenderTargets = 1;
    opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
    opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    opaquePsoDesc.DSVFormat = mDepthStencilFormat;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));


    //
    // PSO for opaque wireframe objects.
    //

    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueWireframePsoDesc = opaquePsoDesc;
    opaqueWireframePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaqueWireframePsoDesc, IID_PPV_ARGS(&mPSOs["opaque_wireframe"])));
}

void ShapesApp::BuildFrameResources()
{
    for (int i = 0; i < gNumFrameResources; ++i)
    {
        mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
            1, (UINT)mAllRitems.size()));
    }
}

// Подготовка объектов для отрисовки
void ShapesApp::BuildRenderItems()
{
    int index = 0;
    for (auto& it : go)
    {
        auto object = std::make_unique<RenderItem>();
        XMStoreFloat4x4(&object->World, XMMatrixScaling(it.second.Scale.X, it.second.Scale.X, 1.0f)
            * XMMatrixTranslation(it.second.Position.X, it.second.Position.Y, 1.0f));
        object->ObjCBIndex = index++;
        object->Geo = mGeometries["shapeGeo"].get();
        object->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        object->IndexCount = object->Geo->DrawArgs[it.second.color].IndexCount;
        object->StartIndexLocation = object->Geo->DrawArgs[it.second.color].StartIndexLocation;
        object->BaseVertexLocation = object->Geo->DrawArgs[it.second.color].BaseVertexLocation;
        mAllRitems[it.first] = std::move(object);
    }

    for (auto& e : mAllRitems)
        mOpaqueRitems.push_back(e.second.get());
}

void ShapesApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

    auto objectCB = mCurrFrameResource->ObjectCB->Resource();

    // For each render item...
    for (size_t i = 0; i < ritems.size(); ++i)
    {
        auto ri = ritems[i];

        cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
        cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
        cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

        // Offset to the CBV in the descriptor heap for this object and for this frame resource.
        UINT cbvIndex = mCurrFrameResourceIndex * (UINT)mOpaqueRitems.size() + ri->ObjCBIndex;
        auto cbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
        cbvHandle.Offset(cbvIndex, mCbvSrvUavDescriptorSize);

        cmdList->SetGraphicsRootDescriptorTable(0, cbvHandle);

        cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
    }
}
