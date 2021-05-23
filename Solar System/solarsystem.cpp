#include "../Common/d3dApp.h"
#include "../Common/MathHelper.h"
#include "../Common/UploadBuffer.h"
#include "../Common/GeometryGenerator.h"
#include "FrameResource.h"

#include <iostream>
#include <string>
#include <map>

#include "ShadowMap.h"

#include "RenderItem.h"
#include "GameInput.h"
#include "Camera.h"
#include "Scene.h"
#include "GameObject.h"
#include "Graphics.h"
#include "Render.h"
#include "Katamari.h"
#include "Planet.h"
#include "Moon.h"
#include "Element.h"
#include "Platform.h"
#include "Universe.h"
#include "Particle.h"
#include "Ssao.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")

const int gNumFrameResources = 3;

void InitializeGameObjects(Scene&);
void InitializeGeometry(ID3D12Device*, ID3D12GraphicsCommandList*, Render&);
void InitializeTextures(ID3D12Device*, ID3D12GraphicsCommandList*, Render&);
void InitializeMaterials(Render&);

class MyEngine : public D3DApp
{
public:
	MyEngine(HINSTANCE hInstance);
	MyEngine(const MyEngine& rhs) = delete;
	MyEngine& operator=(const MyEngine& rhs) = delete;
	~MyEngine();

	virtual bool Initialize()override;

private:
	virtual void CreateRtvAndDsvDescriptorHeaps() override;
	virtual void OnResize()override;
	virtual void GameLoop(const GameTimer& gt) override;
	virtual void Draw(const GameTimer& gt)override;

	virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y) override;

	void UpdateObjectCBs(const GameTimer& Time);
	void UpdateMaterialBuffer(const GameTimer& Time);
	void UpdateShadowTransform(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& Time);
	void UpdateShadowPassCB(const GameTimer& gt);
	void UpdateSsaoCB(const GameTimer& gt);

	void BuildRootSignature();
	void BuildSsaoRootSignature();
	void BuildDescriptorHeaps();
	void InitializeShaders();
	void BuildPSOs();
	void BuildFrameResources();
	void BuildRenderItems();
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);
	void DrawSceneToShadowMap();
	void DrawNormalsAndDepth();

	CD3DX12_CPU_DESCRIPTOR_HANDLE GetCpuSrv(int index)const;
	CD3DX12_GPU_DESCRIPTOR_HANDLE GetGpuSrv(int index)const;
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetDsv(int index)const;
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetRtv(int index)const;

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> GetStaticSamplers();

private:
	// Объекты отрисовки
	std::vector<std::unique_ptr<RenderItem>> mDebugRitems;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mTreeSpriteInputLayout;

	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	ComPtr<ID3D12RootSignature> mSsaoRootSignature = nullptr;

	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;


	// Отрисовка и всё что с этим связано
	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];	// Объекты для отрисовки для различных PSO

	PassConstants mMainPassCB;  // index 0 of pass cbuffer.
	PassConstants mShadowPassCB;// index 1 of pass cbuffer.

	POINT mLastMousePos;		// Последняя позиция мыши

	// Переменные собственного движка
	Scene scene;				// Игровая сцена
	Render render;				// Вспомогательный объект рендеринга

	bool _isWireframe = false;	// Тип отрисовки непрозрачных объектов
	bool _isShadowDebug = false;
	bool _isSsaoDebug = false;

#pragma region Shadow Mapping + SSAO
	std::unique_ptr<ShadowMap> mShadowMap;
	std::unique_ptr<Ssao> mSsao;

	DirectX::BoundingSphere mSceneBounds;

	float mLightNearZ = 0.0f;
	float mLightFarZ = 0.0f;
	XMFLOAT3 mLightPosW;
	XMFLOAT4X4 mLightView = MathHelper::Identity4x4();
	XMFLOAT4X4 mLightProj = MathHelper::Identity4x4();
	XMFLOAT4X4 mShadowTransform = MathHelper::Identity4x4();

	float mLightRotationAngle = 0.0f;
	XMFLOAT3 mBaseLightDirections[3] = {
		XMFLOAT3(0.57735f, -0.57735f, 0.57735f),
		XMFLOAT3(-0.57735f, -0.57735f, 0.57735f),
		XMFLOAT3(0.0f, -0.707f, -0.707f)
	};
	XMFLOAT3 mRotatedLightDirections[3];

	UINT mSkyTexHeapIndex = 0;
	UINT mShadowMapHeapIndex = 0;
	UINT mSsaoHeapIndexStart = 0;
	UINT mSsaoAmbientMapIndex = 0;

	UINT mNullCubeSrvIndex = 0;
	UINT mNullTexSrvIndex1 = 0;
	UINT mNullTexSrvIndex2 = 0;

	CD3DX12_GPU_DESCRIPTOR_HANDLE mNullSrv;
#pragma endregion
};

void MyEngine::CreateRtvAndDsvDescriptorHeaps()
{
	// Add +1 for screen normal map, +2 for ambient maps.
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = _SwapChainBufferCount + 3;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	ThrowIfFailed(_Device->CreateDescriptorHeap(
		&rtvHeapDesc, IID_PPV_ARGS(_DescriptorHeapRTV.GetAddressOf())));

	// Add +1 DSV for shadow map.
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 2;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	ThrowIfFailed(_Device->CreateDescriptorHeap(
		&dsvHeapDesc, IID_PPV_ARGS(_DescriptorHeapDSV.GetAddressOf())));
}

// Точка входа в программу
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd)
{
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		MyEngine theApp(hInstance);
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

// Первичная инициализация игровых объектов
MyEngine::MyEngine(HINSTANCE hInstance)
	: D3DApp(hInstance)
{
	// Область обрезки карты теней
	mSceneBounds.Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
	mSceneBounds.Radius = 100.0f;
}

MyEngine::~MyEngine()
{
	if (_Device != nullptr)
		FlushCommandQueue();
}

bool MyEngine::Initialize()
{
	if (!D3DApp::Initialize())
		return false;

	// Сброс списка команд для подготовки команд инициализации
	ThrowIfFailed(_GraphicsCommandList->Reset(_CommandAllocator.Get(), nullptr));

	mShadowMap = std::make_unique<ShadowMap>(
		_Device.Get(), 2048, 2048);

	mSsao = std::make_unique<Ssao>(
		_Device.Get(),
		_GraphicsCommandList.Get(),
		_ClientWidth, _ClientHeight);



	InitializeGeometry(_Device.Get(), _GraphicsCommandList.Get(), render); // TODO Инициализация геометрии
	InitializeTextures(_Device.Get(), _GraphicsCommandList.Get(), render); // TODO Инициализация геометрии
	InitializeMaterials(render);
	InitializeGameObjects(scene);

	BuildRootSignature();
	BuildSsaoRootSignature();
	BuildDescriptorHeaps();

	BuildRenderItems();
	BuildFrameResources();


	InitializeShaders();
	BuildPSOs();

	mSsao->SetPSOs(mPSOs["ssao"].Get(), mPSOs["ssaoBlur"].Get());

	// Выполнение команд инициализации
	ThrowIfFailed(_GraphicsCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { _GraphicsCommandList.Get() };
	_CommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCommandQueue();
	OnResize();

	return true;
}

void MyEngine::OnResize()
{
	D3DApp::OnResize();

	// Настройка линзы для главной камеры
	if (scene.GetMainCamera() != nullptr)
		scene.GetMainCamera()->SetLens(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 10000000.0f);

	if (mSsao != nullptr)
	{
		mSsao->OnResize(_ClientWidth, _ClientHeight);

		// Resources changed, so need to rebuild descriptors.
		mSsao->RebuildDescriptors(_DepthStencilBuffer.Get());
	}
}

void MyEngine::GameLoop(const GameTimer& gt)
{
	//return;
	if (GameInput::IsKeyPressed(KEYBOARD_F1)) _isWireframe = true;
	if (GameInput::IsKeyPressed(KEYBOARD_F2)) _isWireframe = false;
	if (GameInput::IsKeyPressed(KEYBOARD_F3)) _isShadowDebug = true;
	if (GameInput::IsKeyPressed(KEYBOARD_F4)) _isShadowDebug = false;
	if (GameInput::IsKeyPressed(KEYBOARD_F5)) _isSsaoDebug = true;
	if (GameInput::IsKeyPressed(KEYBOARD_F6)) _isSsaoDebug = false;
	
	// Выполнение Update у всех элементов на сцене
	scene.Update(gt);

	// Выполнение RenderUpdate у всех элементов на сцене
	scene.RenderUpdate();

	// Циклический переход по массиву ресурсов кругового кадра. 
	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	// Закончил ли GPU обрабатывать команды текущего ресурса кадра?
	// Если нет, дождитесь, пока графический процессор завершит команды до этой точки барьера. 
	if (mCurrFrameResource->Fence != 0 && _Fence->GetCompletedValue() < mCurrFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(_Fence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	mLightRotationAngle += 2.0f * gt.DeltaTime();

	XMMATRIX R = XMMatrixRotationY(mLightRotationAngle);
	for (int i = 0; i < 3; ++i)
	{
		XMVECTOR lightDir = XMLoadFloat3(&mBaseLightDirections[i]);
		lightDir = XMVector3TransformNormal(lightDir, R);
		XMStoreFloat3(&mRotatedLightDirections[i], lightDir);
	}

	UpdateObjectCBs(gt);
	UpdateMaterialBuffer(gt);
	UpdateShadowTransform(gt);
	UpdateMainPassCB(gt);
	UpdateShadowPassCB(gt);
	UpdateSsaoCB(gt);
}

void MyEngine::Draw(const GameTimer& gt)
{
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

	// Reuse the memory associated with command recording.
	// We can only reset when the associated command lists have finished execution on the GPU.
	ThrowIfFailed(cmdListAlloc->Reset());

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	ThrowIfFailed(_GraphicsCommandList->Reset(cmdListAlloc.Get(), _isWireframe ? mPSOs["opaque_wireframe"].Get() : mPSOs["opaque"].Get()));

	ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
	_GraphicsCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	_GraphicsCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	// Карта теней

	// Bind all the materials used in this scene.  For structured buffers, we can bypass the heap and 
	// set as a root descriptor.
	auto matBuffer = mCurrFrameResource->MaterialBuffer->Resource();
	_GraphicsCommandList->SetGraphicsRootShaderResourceView(2, matBuffer->GetGPUVirtualAddress());

	// Bind null SRV for shadow map pass.
	_GraphicsCommandList->SetGraphicsRootDescriptorTable(3, mNullSrv);

	// Bind all the textures used in this scene.  Observe
	// that we only have to specify the first descriptor in the table.  
	// The root signature knows how many descriptors are expected in the table.
	_GraphicsCommandList->SetGraphicsRootDescriptorTable(4, mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

	DrawSceneToShadowMap();

	// Карта нормалей/глубины

	DrawNormalsAndDepth();

	//
	// Compute SSAO.
	// 

	_GraphicsCommandList->SetGraphicsRootSignature(mSsaoRootSignature.Get());
	mSsao->ComputeSsao(_GraphicsCommandList.Get(), mCurrFrameResource, 3);

	//
	// Main rendering pass.
	//

	_GraphicsCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	// Rebind state whenever graphics root signature changes.

	// Bind all the materials used in this scene.  For structured buffers, we can bypass the heap and 
	// set as a root descriptor.
	matBuffer = mCurrFrameResource->MaterialBuffer->Resource();
	_GraphicsCommandList->SetGraphicsRootShaderResourceView(2, matBuffer->GetGPUVirtualAddress());

	_GraphicsCommandList->RSSetViewports(1, &_ScreenViewport);
	_GraphicsCommandList->RSSetScissorRects(1, &_ScissorRect);

	// Indicate a state transition on the resource usage.
	_GraphicsCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// Clear the back buffer and depth buffer.
	_GraphicsCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
	
	
	
	
	
	// WE ALREADY WROTE THE DEPTH INFO TO THE DEPTH BUFFER IN DrawNormalsAndDepth,
	// SO DO NOT CLEAR DEPTH.

	// Specify the buffers we are going to render to.
	_GraphicsCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	// Bind all the textures used in this scene.  Observe
	// that we only have to specify the first descriptor in the table.  
	// The root signature knows how many descriptors are expected in the table.
	_GraphicsCommandList->SetGraphicsRootDescriptorTable(4, mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());





	auto passCB = mCurrFrameResource->PassCB->Resource();
	_GraphicsCommandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());

	// Bind the sky cube map.  For our demos, we just use one "world" cube map representing the environment
	// from far away, so all objects will use the same cube map and we only need to set it once per-frame.  
	// If we wanted to use "local" cube maps, we would have to change them per-object, or dynamically
	// index into an array of cube maps.

	CD3DX12_GPU_DESCRIPTOR_HANDLE skyTexDescriptor(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	skyTexDescriptor.Offset(mSkyTexHeapIndex, _DescriptorSizeCSU);
	_GraphicsCommandList->SetGraphicsRootDescriptorTable(3, skyTexDescriptor);

	_GraphicsCommandList->SetPipelineState(_isWireframe ? mPSOs["opaque_wireframe"].Get() : mPSOs["opaque"].Get());
	DrawRenderItems(_GraphicsCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);

	_GraphicsCommandList->SetPipelineState(mPSOs["billboardSprites"].Get());
	DrawRenderItems(_GraphicsCommandList.Get(), mRitemLayer[(int)RenderLayer::AlphaTestedTreeSprites]);

	if (_isShadowDebug)
	{
		_GraphicsCommandList->SetPipelineState(mPSOs["ShadowDebug"].Get());
		DrawRenderItems(_GraphicsCommandList.Get(), mRitemLayer[(int)RenderLayer::ShadowDebug]);
	}

	if (_isSsaoDebug)
	{
		_GraphicsCommandList->SetPipelineState(mPSOs["SsaoDebug"].Get());
		DrawRenderItems(_GraphicsCommandList.Get(), mRitemLayer[(int)RenderLayer::SsaoDebug]);
	}

	//_GraphicsCommandList->SetPipelineState(mPSOs["sky"].Get());
	//DrawRenderItems(_GraphicsCommandList.Get(), mRitemLayer[(int)RenderLayer::Sky]);

	// Indicate a state transition on the resource usage.
	_GraphicsCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	// Done recording commands.
	ThrowIfFailed(_GraphicsCommandList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { _GraphicsCommandList.Get() };
	_CommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Swap the back and front buffers
	ThrowIfFailed(_SwapChain->Present(0, 0));
	_CurrentBackBufferIndex = (_CurrentBackBufferIndex + 1) % _SwapChainBufferCount;

	// Advance the fence value to mark commands up to this fence point.
	mCurrFrameResource->Fence = ++_CurrentFenceIndex;

	// Add an instruction to the command queue to set a new fence point. 
	// Because we are on the GPU timeline, the new fence point won't be 
	// set until the GPU finishes processing all the commands prior to this Signal().
	_CommandQueue->Signal(_Fence.Get(), _CurrentFenceIndex);
}

#pragma region Управление мышью
// Запоминание координаты клика
void MyEngine::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mhMainWnd);
}

void MyEngine::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

// Мышь полностью отдана под управление камерой
void MyEngine::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_RBUTTON) != 0)
	{
		// Каждый пройденный пиксель соответствует четверти градуса
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

		if (scene.GetMainCamera() != nullptr) {
			scene.GetMainCamera()->Pitch(dy);
			scene.GetMainCamera()->RotateY(dx);
		}
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}
#pragma endregion

// Обновляем данные из объектов рендеринга в константных буферах
void MyEngine::UpdateObjectCBs(const GameTimer& Time)
{
	// Получаем константный буфер
	auto currObjectCB = mCurrFrameResource->ObjectCB.get();

	// Обновляем данные cbuffer, только если константы изменились (это необходимо отслеживать для каждого кадра)
	//if (e.second->NumFramesDirty > 0) TODO
	
	// Обновляем объекты
	for (auto& it : scene.GetAllGameObjects())
	{
		GameObject* go = it.second;
		RenderItem* ri = go->ri.get();

		// TODO Обновление только тех объектов, которые были изменены
		if (!go->Transform.IsDirty())
			continue;

		XMMATRIX world = XMLoadFloat4x4(&ri->World);
		XMMATRIX texTransform = XMLoadFloat4x4(&ri->TexTransform);

		ObjectConstants objConstants;
		XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
		XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));
		objConstants.MaterialIndex = ri->Mat->MatCBIndex;

		currObjectCB->CopyData(ri->ObjCBIndex, objConstants);

		ri->NumFramesDirty--;
	}
}

void MyEngine::UpdateMaterialBuffer(const GameTimer& Time)
{
	auto currMaterialBuffer = mCurrFrameResource->MaterialBuffer.get();
	for (auto& e : render.GetMaterialMap())
	{
		// Only update the cbuffer data if the constants have changed.  If the cbuffer
		// data changes, it needs to be updated for each FrameResource.
		Material* mat = e.second.get();
		if (mat->NumFramesDirty > 0)
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialData matData;
			matData.DiffuseAlbedo = mat->DiffuseAlbedo;
			matData.FresnelR0 = mat->FresnelR0;
			matData.Roughness = mat->Roughness;
			XMStoreFloat4x4(&matData.MatTransform, XMMatrixTranspose(matTransform));
			matData.DiffuseMapIndex = mat->DiffuseSrvHeapIndex;
			matData.NormalMapIndex = mat->NormalSrvHeapIndex;

			currMaterialBuffer->CopyData(mat->MatCBIndex, matData);

			// Next FrameResource need to be updated too.
			mat->NumFramesDirty--;
		}
	}
}

void MyEngine::UpdateShadowTransform(const GameTimer& gt)
{
	// Only the first "main" light casts a shadow.
	XMVECTOR lightDir = XMLoadFloat3(&mRotatedLightDirections[0]);
	XMVECTOR lightPos = -2.0f * mSceneBounds.Radius * lightDir;
	XMVECTOR targetPos = XMLoadFloat3(&mSceneBounds.Center);
	XMVECTOR lightUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMMATRIX lightView = XMMatrixLookAtLH(lightPos, targetPos, lightUp);

	XMStoreFloat3(&mLightPosW, lightPos);

	// Transform bounding sphere to light space.
	XMFLOAT3 sphereCenterLS;
	XMStoreFloat3(&sphereCenterLS, XMVector3TransformCoord(targetPos, lightView));

	// Ortho frustum in light space encloses scene.
	float l = sphereCenterLS.x - mSceneBounds.Radius;
	float b = sphereCenterLS.y - mSceneBounds.Radius;
	float n = sphereCenterLS.z - mSceneBounds.Radius;
	float r = sphereCenterLS.x + mSceneBounds.Radius;
	float t = sphereCenterLS.y + mSceneBounds.Radius;
	float f = sphereCenterLS.z + mSceneBounds.Radius;

	mLightNearZ = n;
	mLightFarZ = f;
	XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);

	// Transform NDC space [-1,+1]^2 to texture space [0,1]^2
	XMMATRIX T(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f);

	XMMATRIX S = lightView * lightProj * T;
	XMStoreFloat4x4(&mLightView, lightView);
	XMStoreFloat4x4(&mLightProj, lightProj);
	XMStoreFloat4x4(&mShadowTransform, S);
}

void MyEngine::UpdateMainPassCB(const GameTimer& Time)
{
	XMMATRIX view;
	XMMATRIX proj;
	if (scene.GetMainCamera() != nullptr) 
	{
		view = scene.GetMainCamera()->GetView();
		proj = scene.GetMainCamera()->GetProj();
	}
	else
	{
		view = XMMatrixSet(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
		proj = XMMatrixSet(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	}

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);
	
	// Transform NDC space [-1,+1]^2 to texture space [0,1]^2
	XMMATRIX T(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f);

	XMMATRIX viewProjTex = XMMatrixMultiply(viewProj, T);
	XMMATRIX shadowTransform = XMLoadFloat4x4(&mShadowTransform);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProjTex, XMMatrixTranspose(viewProjTex));
	XMStoreFloat4x4(&mMainPassCB.ShadowTransform, XMMatrixTranspose(shadowTransform));
	if (scene.GetMainCamera() != nullptr)
	{
		mMainPassCB.EyePosW = scene.GetMainCamera()->GetPosition3f();
	}
	else
	{
		mMainPassCB.EyePosW = XMFLOAT3(0.0f, 0.0f, 0.0f);
	}
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)_ClientWidth, (float)_ClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / _ClientWidth, 1.0f / _ClientHeight);
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = 1000.0f;
	mMainPassCB.TotalTime = Time.TotalTime();
	mMainPassCB.DeltaTime = Time.DeltaTime();

	// Настройка источников света
	mMainPassCB.AmbientLight = { 0.5f, 0.5f, 0.5f, 1.0f };
	mMainPassCB.Lights[0].Direction = mRotatedLightDirections[0];
	mMainPassCB.Lights[0].Strength = { 0.9f, 0.9f, 0.9f };
	mMainPassCB.Lights[0].Position = { 0.0f, 3.0f, 0.0f };
	mMainPassCB.Lights[1].Direction = mRotatedLightDirections[1];
	mMainPassCB.Lights[1].Strength = { 0.4f, 0.4f, 0.4f };
	mMainPassCB.Lights[2].Direction = mRotatedLightDirections[2];
	mMainPassCB.Lights[2].Strength = { 0.2f, 0.2f, 0.2f };

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

void MyEngine::UpdateShadowPassCB(const GameTimer& gt)
{
	XMMATRIX view = XMLoadFloat4x4(&mLightView);
	XMMATRIX proj = XMLoadFloat4x4(&mLightProj);

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	UINT w = mShadowMap->Width();
	UINT h = mShadowMap->Height();

	XMStoreFloat4x4(&mShadowPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mShadowPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mShadowPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mShadowPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mShadowPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mShadowPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mShadowPassCB.EyePosW = mLightPosW;
	mShadowPassCB.RenderTargetSize = XMFLOAT2((float)w, (float)h);
	mShadowPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / w, 1.0f / h);
	mShadowPassCB.NearZ = mLightNearZ;
	mShadowPassCB.FarZ = mLightFarZ;

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(1, mShadowPassCB);
}

void MyEngine::UpdateSsaoCB(const GameTimer& gt)
{
	SsaoConstants ssaoCB;

	XMMATRIX P = scene.GetMainCamera()->GetProj();

	// Transform NDC space [-1,+1]^2 to texture space [0,1]^2
	XMMATRIX T(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f);

	ssaoCB.Proj = mMainPassCB.Proj;
	ssaoCB.InvProj = mMainPassCB.InvProj;
	XMStoreFloat4x4(&ssaoCB.ProjTex, XMMatrixTranspose(P * T));

	mSsao->GetOffsetVectors(ssaoCB.OffsetVectors);

	auto blurWeights = mSsao->CalcGaussWeights(2.5f);
	ssaoCB.BlurWeights[0] = XMFLOAT4(&blurWeights[0]);
	ssaoCB.BlurWeights[1] = XMFLOAT4(&blurWeights[4]);
	ssaoCB.BlurWeights[2] = XMFLOAT4(&blurWeights[8]);

	ssaoCB.InvRenderTargetSize = XMFLOAT2(1.0f / mSsao->SsaoMapWidth(), 1.0f / mSsao->SsaoMapHeight());

	// Coordinates given in view space.
	ssaoCB.OcclusionRadius = 0.5f;
	ssaoCB.OcclusionFadeStart = 0.2f;
	ssaoCB.OcclusionFadeEnd = 1.0f;
	ssaoCB.SurfaceEpsilon = 0.05f;

	auto currSsaoCB = mCurrFrameResource->SsaoCB.get();
	currSsaoCB->CopyData(0, ssaoCB);
}

void MyEngine::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE texTable0;
	texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0, 0);

	CD3DX12_DESCRIPTOR_RANGE texTable1;
	texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 50, 3, 0);	// 25 is count of textures

	// Корневой параметр может быть таблицей, корневым дескриптором или корневыми константами
	CD3DX12_ROOT_PARAMETER slotRootParameter[5];

	// Для производительности располагаем по частоте использования (от большего к меньшему)
	slotRootParameter[0].InitAsConstantBufferView(0);
	slotRootParameter[1].InitAsConstantBufferView(1);
	slotRootParameter[2].InitAsShaderResourceView(0, 1);
	slotRootParameter[3].InitAsDescriptorTable(1, &texTable0, D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[4].InitAsDescriptorTable(1, &texTable1, D3D12_SHADER_VISIBILITY_PIXEL);

	// Получение статических сэмплеров
	auto staticSamplers = GetStaticSamplers();

	// Корневая подпись - это массив корневых параметров
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(5, slotRootParameter,
		(UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// Создание корневой подписи с одним слотом,
	// который указывает на диапазон дескрипторов
	// состоящий из одного постоянного буфера
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(_Device->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void MyEngine::BuildSsaoRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE texTable0;
	texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0);

	CD3DX12_DESCRIPTOR_RANGE texTable1;
	texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[4];

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsConstantBufferView(0);
	slotRootParameter[1].InitAsConstants(1, 1);
	slotRootParameter[2].InitAsDescriptorTable(1, &texTable0, D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[3].InitAsDescriptorTable(1, &texTable1, D3D12_SHADER_VISIBILITY_PIXEL);

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC depthMapSam(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressW
		0.0f,
		0,
		D3D12_COMPARISON_FUNC_LESS_EQUAL,
		D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE);

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	std::array<CD3DX12_STATIC_SAMPLER_DESC, 4> staticSamplers =
	{
		pointClamp, linearClamp, depthMapSam, linearWrap
	};

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter,
		(UINT)staticSamplers.size(), staticSamplers.data(),
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

	ThrowIfFailed(_Device->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mSsaoRootSignature.GetAddressOf())));
}

// Подготовка дескрипторов кучи SRV для текстур
void MyEngine::BuildDescriptorHeaps()
{
	// Создание дескриптора кучи SRV
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = render.GetTextureMap().size() * 3;		// Зависимость от LoadTexture
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(_Device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

	// Заполнение кучи
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	for (int i = 0; i < render.GetTextureMap().size(); ++i)
	{
		srvDesc.Format = render.GetTextureMap()[i]->Resource->GetDesc().Format;
		srvDesc.Texture2D.MipLevels = render.GetTextureMap()[i]->Resource->GetDesc().MipLevels;
		_Device->CreateShaderResourceView(render.GetTextureMap()[i]->Resource.Get(), &srvDesc, hDescriptor);

		// Переход к следующему дескриптору
		hDescriptor.Offset(1, _DescriptorSizeCSU);
	}






	mSkyTexHeapIndex = (UINT)render.GetTextureMap().size() - 1;		// -1 т.к. size
	mShadowMapHeapIndex = (UINT)render.GetTextureMap().size();
	mSsaoHeapIndexStart = mShadowMapHeapIndex + 1;
	mSsaoAmbientMapIndex = mSsaoHeapIndexStart + 3;
	mNullCubeSrvIndex = mSsaoHeapIndexStart + 5;
	mNullTexSrvIndex1 = mNullCubeSrvIndex + 1;
	mNullTexSrvIndex2 = mNullTexSrvIndex1 + 1;






	auto nullSrv = GetCpuSrv(mNullCubeSrvIndex);
	mNullSrv = GetGpuSrv(mNullCubeSrvIndex);

	_Device->CreateShaderResourceView(nullptr, &srvDesc, nullSrv);
	nullSrv.Offset(1, _DescriptorSizeCSU);

	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	_Device->CreateShaderResourceView(nullptr, &srvDesc, nullSrv);

	nullSrv.Offset(1, _DescriptorSizeCSU);
	_Device->CreateShaderResourceView(nullptr, &srvDesc, nullSrv);

	mShadowMap->BuildDescriptors(
		GetCpuSrv(mShadowMapHeapIndex),
		GetGpuSrv(mShadowMapHeapIndex),
		GetDsv(1));

	mSsao->BuildDescriptors(
		_DepthStencilBuffer.Get(),
		GetCpuSrv(mSsaoHeapIndexStart),
		GetGpuSrv(mSsaoHeapIndexStart),
		GetRtv(_SwapChainBufferCount),
		_DescriptorSizeCSU,
		_DescriptorSizeRTV);
}

void MyEngine::BuildPSOs()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC basePsoDesc;

	//
	// PSO for opaque objects.
	//
	ZeroMemory(&basePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	basePsoDesc.InputLayout = { render.GetShaderInputLayout().data(), (UINT)render.GetShaderInputLayout().size() };
	basePsoDesc.pRootSignature = mRootSignature.Get();
	basePsoDesc.VS = render.GetShaderBytecode("defaultVS");
	basePsoDesc.PS = render.GetShaderBytecode("defaultPS");
	basePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	basePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	basePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	basePsoDesc.SampleMask = UINT_MAX;
	basePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	basePsoDesc.NumRenderTargets = 1;
	basePsoDesc.RTVFormats[0] = _BackBufferFormat;
	basePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	basePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	basePsoDesc.DSVFormat = _DepthStencilFormat;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc = basePsoDesc;
	opaquePsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_EQUAL;
	opaquePsoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	ThrowIfFailed(_Device->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueWireframePsoDesc = basePsoDesc;
	opaqueWireframePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	ThrowIfFailed(_Device->CreateGraphicsPipelineState(&opaqueWireframePsoDesc, IID_PPV_ARGS(&mPSOs["opaque_wireframe"])));

	//
	// PSO for transparent objects
	//

	D3D12_GRAPHICS_PIPELINE_STATE_DESC transparentPsoDesc = basePsoDesc;

	D3D12_RENDER_TARGET_BLEND_DESC transparencyBlendDesc;
	transparencyBlendDesc.BlendEnable = true;
	transparencyBlendDesc.LogicOpEnable = false;
	transparencyBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	transparencyBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	transparencyBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	transparencyBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	transparencyBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	transparencyBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	transparentPsoDesc.BlendState.RenderTarget[0] = transparencyBlendDesc;
	ThrowIfFailed(_Device->CreateGraphicsPipelineState(&transparentPsoDesc, IID_PPV_ARGS(&mPSOs["transparent"])));

	//
	// PSO for marking stencil mirrors.
	//

	CD3DX12_BLEND_DESC mirrorBlendState(D3D12_DEFAULT);
	mirrorBlendState.RenderTarget[0].RenderTargetWriteMask = 0;

	D3D12_DEPTH_STENCIL_DESC mirrorDSS;
	mirrorDSS.DepthEnable = true;
	mirrorDSS.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	mirrorDSS.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	mirrorDSS.StencilEnable = true;
	mirrorDSS.StencilReadMask = 0xff;
	mirrorDSS.StencilWriteMask = 0xff;

	mirrorDSS.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	mirrorDSS.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	mirrorDSS.FrontFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
	mirrorDSS.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	// We are not rendering backfacing polygons, so these settings do not matter.
	mirrorDSS.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	mirrorDSS.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	mirrorDSS.BackFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
	mirrorDSS.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC markMirrorsPsoDesc = basePsoDesc;
	markMirrorsPsoDesc.BlendState = mirrorBlendState;
	markMirrorsPsoDesc.DepthStencilState = mirrorDSS;
	ThrowIfFailed(_Device->CreateGraphicsPipelineState(&markMirrorsPsoDesc, IID_PPV_ARGS(&mPSOs["markStencilMirrors"])));

	//
	// PSO for stencil reflections.
	//

	D3D12_DEPTH_STENCIL_DESC reflectionsDSS;
	reflectionsDSS.DepthEnable = true;
	reflectionsDSS.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	reflectionsDSS.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	reflectionsDSS.StencilEnable = true;
	reflectionsDSS.StencilReadMask = 0xff;
	reflectionsDSS.StencilWriteMask = 0xff;

	reflectionsDSS.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	reflectionsDSS.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	reflectionsDSS.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	reflectionsDSS.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;

	// We are not rendering backfacing polygons, so these settings do not matter.
	reflectionsDSS.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	reflectionsDSS.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	reflectionsDSS.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	reflectionsDSS.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC drawReflectionsPsoDesc = basePsoDesc;
	drawReflectionsPsoDesc.DepthStencilState = reflectionsDSS;
	drawReflectionsPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	drawReflectionsPsoDesc.RasterizerState.FrontCounterClockwise = true;
	ThrowIfFailed(_Device->CreateGraphicsPipelineState(&drawReflectionsPsoDesc, IID_PPV_ARGS(&mPSOs["drawStencilReflections"])));

	//
	// PSO for shadow objects
	//

	// We are going to draw shadows with transparency, so base it off the transparency description.
	D3D12_DEPTH_STENCIL_DESC shadowDSS;
	shadowDSS.DepthEnable = true;
	shadowDSS.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	shadowDSS.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	shadowDSS.StencilEnable = true;
	shadowDSS.StencilReadMask = 0xff;
	shadowDSS.StencilWriteMask = 0xff;

	shadowDSS.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	shadowDSS.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	shadowDSS.FrontFace.StencilPassOp = D3D12_STENCIL_OP_INCR;
	shadowDSS.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;

	// We are not rendering backfacing polygons, so these settings do not matter.
	shadowDSS.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	shadowDSS.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	shadowDSS.BackFace.StencilPassOp = D3D12_STENCIL_OP_INCR;
	shadowDSS.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC shadowPsoDesc = transparentPsoDesc;
	shadowPsoDesc.DepthStencilState = shadowDSS;
	ThrowIfFailed(_Device->CreateGraphicsPipelineState(&shadowPsoDesc, IID_PPV_ARGS(&mPSOs["shadow"])));

	//
	// PSO for shadow map pass.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC smapPsoDesc = basePsoDesc;
	smapPsoDesc.RasterizerState.DepthBias = 100000;
	smapPsoDesc.RasterizerState.DepthBiasClamp = 0.0f;
	smapPsoDesc.RasterizerState.SlopeScaledDepthBias = 1.0f;
	smapPsoDesc.pRootSignature = mRootSignature.Get();
	smapPsoDesc.VS = render.GetShaderBytecode("shadowVS");
	smapPsoDesc.PS = render.GetShaderBytecode("shadowOpaquePS");

	// Shadow map pass does not have a render target.
	smapPsoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
	smapPsoDesc.NumRenderTargets = 0;
	ThrowIfFailed(_Device->CreateGraphicsPipelineState(&smapPsoDesc, IID_PPV_ARGS(&mPSOs["shadow_opaque"])));

	//
	// PSO for debug layer.
	//
	//D3D12_GRAPHICS_PIPELINE_STATE_DESC debugPsoDesc = basePsoDesc;
	//debugPsoDesc.pRootSignature = mRootSignature.Get();
	//debugPsoDesc.VS = render.GetShaderBytecode("debugVS");
	//debugPsoDesc.PS = render.GetShaderBytecode("debugPS");
	//ThrowIfFailed(_Device->CreateGraphicsPipelineState(&debugPsoDesc, IID_PPV_ARGS(&mPSOs["debug"])));

	//
// PSO for sky.
//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC skyPsoDesc = basePsoDesc;

	// The camera is inside the sky sphere, so just turn off culling.
	skyPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

	// Make sure the depth function is LESS_EQUAL and not just LESS.  
	// Otherwise, the normalized depth values at z = 1 (NDC) will 
	// fail the depth test if the depth buffer was cleared to 1.
	skyPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	skyPsoDesc.pRootSignature = mRootSignature.Get();
	skyPsoDesc.VS = render.GetShaderBytecode("skyVS");
	skyPsoDesc.PS = render.GetShaderBytecode("skyPS");
	ThrowIfFailed(_Device->CreateGraphicsPipelineState(&skyPsoDesc, IID_PPV_ARGS(&mPSOs["sky"])));

	//
// PSO for drawing normals.
//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC drawNormalsPsoDesc = basePsoDesc;
	drawNormalsPsoDesc.VS = render.GetShaderBytecode("drawNormalsVS");
	drawNormalsPsoDesc.PS = render.GetShaderBytecode("drawNormalsPS");
	drawNormalsPsoDesc.RTVFormats[0] = Ssao::NormalMapFormat;
	drawNormalsPsoDesc.SampleDesc.Count = 1;
	drawNormalsPsoDesc.SampleDesc.Quality = 0;
	drawNormalsPsoDesc.DSVFormat = _DepthStencilFormat;
	ThrowIfFailed(_Device->CreateGraphicsPipelineState(&drawNormalsPsoDesc, IID_PPV_ARGS(&mPSOs["drawNormals"])));

	//
	// PSO for SSAO.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC ssaoPsoDesc = basePsoDesc;
	ssaoPsoDesc.InputLayout = { nullptr, 0 };
	ssaoPsoDesc.pRootSignature = mSsaoRootSignature.Get();
	ssaoPsoDesc.VS = render.GetShaderBytecode("ssaoVS");
	ssaoPsoDesc.PS = render.GetShaderBytecode("ssaoPS");

	// SSAO effect does not need the depth buffer.
	ssaoPsoDesc.DepthStencilState.DepthEnable = false;
	ssaoPsoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	ssaoPsoDesc.RTVFormats[0] = Ssao::AmbientMapFormat;
	ssaoPsoDesc.SampleDesc.Count = 1;
	ssaoPsoDesc.SampleDesc.Quality = 0;
	ssaoPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	ThrowIfFailed(_Device->CreateGraphicsPipelineState(&ssaoPsoDesc, IID_PPV_ARGS(&mPSOs["ssao"])));

	//
	// PSO for SSAO blur.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC ssaoBlurPsoDesc = ssaoPsoDesc;
	ssaoBlurPsoDesc.VS = render.GetShaderBytecode("ssaoBlurVS");
	ssaoBlurPsoDesc.PS = render.GetShaderBytecode("ssaoBlurPS");
	ThrowIfFailed(_Device->CreateGraphicsPipelineState(&ssaoBlurPsoDesc, IID_PPV_ARGS(&mPSOs["ssaoBlur"])));

	// PSO for Shadow Quad
	D3D12_GRAPHICS_PIPELINE_STATE_DESC quadPsoDesc = basePsoDesc;
	quadPsoDesc.pRootSignature = mRootSignature.Get();
	quadPsoDesc.VS = render.GetShaderBytecode("shadowQuadVS");
	quadPsoDesc.PS = render.GetShaderBytecode("shadowQuadPS");
	ThrowIfFailed(_Device->CreateGraphicsPipelineState(&quadPsoDesc, IID_PPV_ARGS(&mPSOs["ShadowDebug"])));

	// PSO for SSAO Quad
	D3D12_GRAPHICS_PIPELINE_STATE_DESC quad2PsoDesc = basePsoDesc;
	quad2PsoDesc.pRootSignature = mRootSignature.Get();
	quad2PsoDesc.VS = render.GetShaderBytecode("ssaoQuadVS");
	quad2PsoDesc.PS = render.GetShaderBytecode("ssaoQuadPS");
	ThrowIfFailed(_Device->CreateGraphicsPipelineState(&quad2PsoDesc, IID_PPV_ARGS(&mPSOs["SsaoDebug"])));


	//
	// PSO for tree sprites
	//

	D3D12_GRAPHICS_PIPELINE_STATE_DESC treeSpritePsoDesc = basePsoDesc;
	treeSpritePsoDesc.VS = render.GetShaderBytecode("billboardVS");
	treeSpritePsoDesc.GS = render.GetShaderBytecode("billboardGS");
	treeSpritePsoDesc.PS = render.GetShaderBytecode("billboardPS");
	treeSpritePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	treeSpritePsoDesc.InputLayout = { mTreeSpriteInputLayout.data(), (UINT)mTreeSpriteInputLayout.size() };
	treeSpritePsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	ThrowIfFailed(_Device->CreateGraphicsPipelineState(&treeSpritePsoDesc, IID_PPV_ARGS(&mPSOs["billboardSprites"])));
}

void MyEngine::BuildFrameResources()
{
	for (int i = 0; i < gNumFrameResources; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(
			_Device.Get(), 
			2,										// pass count
			(UINT)scene.GetAllGameObjects().size() + 2, 
			(UINT)render.GetMaterialMap().size())
		);
	}
}

void MyEngine::BuildRenderItems()
{
	int index = 0;
	for (auto& it : scene.GetAllGameObjects())
	{
		// Извлечение игрового объекта для удобства
		GameObject* go = it.second;

		auto objectRitem = std::make_unique<RenderItem>();
		XMStoreFloat4x4(&objectRitem->World, XMMatrixTranslation(go->Transform.Position.X, go->Transform.Position.Y, go->Transform.Position.Z));
		objectRitem->ObjCBIndex = index;
		objectRitem->Geo = render.GetGeometry(go->Geometry);	// Установка геометрии объекту
		objectRitem->Mat = render.GetMaterial(go->Material);	// Установка материала объекту
		objectRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		objectRitem->IndexCount = objectRitem->Geo->DrawArgs[go->Geometry].IndexCount;
		objectRitem->StartIndexLocation = objectRitem->Geo->DrawArgs[go->Geometry].StartIndexLocation;
		objectRitem->BaseVertexLocation = objectRitem->Geo->DrawArgs[go->Geometry].BaseVertexLocation;

		// Добавление объекта в слой рендеринга
		mRitemLayer[(int)go->RenderLayer].push_back(objectRitem.get());

		// Добавление объекта в список рендера
		go->ri = std::move(objectRitem);

		index++;
	}

	// Отрисовка остальных объектов
	auto quadRitem = std::make_unique<RenderItem>();
	quadRitem->World = MathHelper::Identity4x4();
	quadRitem->TexTransform = MathHelper::Identity4x4();
	quadRitem->ObjCBIndex = index;
	quadRitem->Geo = render.GetGeometry("DebugQuadLD");
	quadRitem->Mat = render.GetMaterial("debug");
	quadRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	quadRitem->IndexCount = quadRitem->Geo->DrawArgs["DebugQuadLD"].IndexCount;
	quadRitem->StartIndexLocation = quadRitem->Geo->DrawArgs["DebugQuadLD"].StartIndexLocation;
	quadRitem->BaseVertexLocation = quadRitem->Geo->DrawArgs["DebugQuadLD"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::ShadowDebug].push_back(quadRitem.get());
	mDebugRitems.push_back(std::move(quadRitem));

	// Отрисовка остальных объектов
	auto quadRitem2 = std::make_unique<RenderItem>();
	quadRitem2->World = MathHelper::Identity4x4();
	quadRitem2->TexTransform = MathHelper::Identity4x4();
	quadRitem2->ObjCBIndex = index + 1;
	quadRitem2->Geo = render.GetGeometry("DebugQuadRD");
	quadRitem2->Mat = render.GetMaterial("debug");
	quadRitem2->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	quadRitem2->IndexCount = quadRitem2->Geo->DrawArgs["DebugQuadRD"].IndexCount;
	quadRitem2->StartIndexLocation = quadRitem2->Geo->DrawArgs["DebugQuadRD"].StartIndexLocation;
	quadRitem2->BaseVertexLocation = quadRitem2->Geo->DrawArgs["DebugQuadRD"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::SsaoDebug].push_back(quadRitem2.get());
	mDebugRitems.push_back(std::move(quadRitem2));
}

void MyEngine::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	auto objectCB = mCurrFrameResource->ObjectCB->Resource();

	// Для всех объектов отрисовки
	for (size_t i = 0; i < ritems.size(); ++i)
	{
		auto ri = ritems[i];

		cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;

		cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress);

		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}

void MyEngine::DrawSceneToShadowMap()
{
	_GraphicsCommandList->RSSetViewports(1, &mShadowMap->Viewport());
	_GraphicsCommandList->RSSetScissorRects(1, &mShadowMap->ScissorRect());

	// Change to DEPTH_WRITE.
	_GraphicsCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mShadowMap->Resource(),
		D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE));

	UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants));

	// Clear the back buffer and depth buffer.
	_GraphicsCommandList->ClearDepthStencilView(mShadowMap->Dsv(),
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Set null render target because we are only going to draw to
	// depth buffer.  Setting a null render target will disable color writes.
	// Note the active PSO also must specify a render target count of 0.
	_GraphicsCommandList->OMSetRenderTargets(0, nullptr, false, &mShadowMap->Dsv());

	// Bind the pass constant buffer for the shadow map pass.
	auto passCB = mCurrFrameResource->PassCB->Resource();
	D3D12_GPU_VIRTUAL_ADDRESS passCBAddress = passCB->GetGPUVirtualAddress() + 1 * passCBByteSize;
	_GraphicsCommandList->SetGraphicsRootConstantBufferView(1, passCBAddress);

	_GraphicsCommandList->SetPipelineState(mPSOs["shadow_opaque"].Get());

	DrawRenderItems(_GraphicsCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);

	// Change back to GENERIC_READ so we can read the texture in a shader.
	_GraphicsCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mShadowMap->Resource(),
		D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ));
}

void MyEngine::DrawNormalsAndDepth()
{
	_GraphicsCommandList->RSSetViewports(1, &_ScreenViewport);
	_GraphicsCommandList->RSSetScissorRects(1, &_ScissorRect);

	auto normalMap = mSsao->NormalMap();
	auto normalMapRtv = mSsao->NormalMapRtv();

	// Change to RENDER_TARGET.
	_GraphicsCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(normalMap,
		D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// Clear the screen normal map and depth buffer.
	float clearValue[] = { 0.0f, 0.0f, 1.0f, 0.0f };
	_GraphicsCommandList->ClearRenderTargetView(normalMapRtv, clearValue, 0, nullptr);
	_GraphicsCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Specify the buffers we are going to render to.
	_GraphicsCommandList->OMSetRenderTargets(1, &normalMapRtv, true, &DepthStencilView());

	// Bind the constant buffer for this pass.
	auto passCB = mCurrFrameResource->PassCB->Resource();
	_GraphicsCommandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());

	_GraphicsCommandList->SetPipelineState(mPSOs["drawNormals"].Get());

	DrawRenderItems(_GraphicsCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);

	// Change back to GENERIC_READ so we can read the texture in a shader.
	_GraphicsCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(normalMap,
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));
}

CD3DX12_CPU_DESCRIPTOR_HANDLE MyEngine::GetCpuSrv(int index) const
{
	auto srv = CD3DX12_CPU_DESCRIPTOR_HANDLE(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	srv.Offset(index, _DescriptorSizeCSU);
	return srv;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE MyEngine::GetGpuSrv(int index) const
{
	auto srv = CD3DX12_GPU_DESCRIPTOR_HANDLE(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	srv.Offset(index, _DescriptorSizeCSU);
	return srv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE MyEngine::GetDsv(int index) const
{
	auto dsv = CD3DX12_CPU_DESCRIPTOR_HANDLE(_DescriptorHeapDSV->GetCPUDescriptorHandleForHeapStart());
	dsv.Offset(index, _DescriptorSizeDSV);
	return dsv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE MyEngine::GetRtv(int index) const
{
	auto rtv = CD3DX12_CPU_DESCRIPTOR_HANDLE(_DescriptorHeapRTV->GetCPUDescriptorHandleForHeapStart());
	rtv.Offset(index, _DescriptorSizeRTV);
	return rtv;
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> MyEngine::GetStaticSamplers()
{
	// Applications usually only need a handful of samplers.  So just define them all up front
	// and keep them available as part of the root signature.  

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC shadow(
		6, // shaderRegister
		D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressW
		0.0f,                               // mipLODBias
		16,                                 // maxAnisotropy
		D3D12_COMPARISON_FUNC_LESS_EQUAL,
		D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK);

	return {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp,
		shadow
	};
}

#pragma region Игровая логика
void InitializeGeometry(ID3D12Device* device, ID3D12GraphicsCommandList* gcl, Render& render)
{
	GeometryGenerator geoGen;
	render.SetGeometry(device, gcl, "SphereGeo",	geoGen.CreateSphere(1.0f, 32, 32));
	render.SetGeometry(device, gcl, "SkysphereGeo", geoGen.CreateSkysphere(1.0f, 32, 32));
	render.SetGeometry(device, gcl, "GridGeo",		geoGen.CreateGrid(5.0f, 5.0f, 60, 60));
	render.SetGeometry(device, gcl, "DebugQuadRD",	geoGen.CreateQuad(0.5f, -0.5f, 0.5f, 0.5f, 0.0f));
	render.SetGeometry(device, gcl, "DebugQuadLD",	geoGen.CreateQuad(-1.0f, -0.5f, 0.5f, 0.5f, 0.0f));
}

void InitializeTextures(ID3D12Device* device, ID3D12GraphicsCommandList* gcl, Render& render)
{
	render.SetTexture(device, gcl,			"UniverseDiffuseMap",		L"../Textures/SolarSystem/MilkyWayColor.dds");
	render.SetTexture(device, gcl,			"SunDiffuseMap",			L"../Textures/SolarSystem/SunColor.dds");
	render.SetTexture(device, gcl,			"MercuryDiffuseMap",		L"../Textures/SolarSystem/MercuryColor.dds");
	render.SetTexture(device, gcl,			"VenusDiffuseMap",			L"../Textures/SolarSystem/VenusColor.dds");
	render.SetTexture(device, gcl,			"EarthDiffuseMap",			L"../Textures/SolarSystem/EarthColor.dds");
	render.SetTexture(device, gcl,			"MoonDiffuseMap",			L"../Textures/SolarSystem/MoonColor.dds");
	render.SetTexture(device, gcl,			"MarsDiffuseMap",			L"../Textures/SolarSystem/MarsColor.dds");
	render.SetTexture(device, gcl,			"JupiterDiffuseMap",		L"../Textures/SolarSystem/JupiterColor.dds");
	render.SetTexture(device, gcl,			"SaturnDiffuseMap",			L"../Textures/SolarSystem/SaturnColor.dds");
	render.SetTexture(device, gcl,			"UranusDiffuseMap",			L"../Textures/SolarSystem/UranusColor.dds");
	render.SetTexture(device, gcl,			"NeptuneDiffuseMap",		L"../Textures/SolarSystem/NeptuneColor.dds");
	render.SetTexture(device, gcl,			"sprite",					L"../Textures/SolarSystem/treeArray2.dds");
	render.SetTexture(device, gcl,			"ds",						L"../Textures/SolarSystem/ds2.dds");

	render.SetTexture(device, gcl,			"MercuryNormalMap",			L"../Textures/SolarSystem/Mercury_NRM.dds");
	render.SetTexture(device, gcl,			"VenusNormalMap",			L"../Textures/SolarSystem/Venus_NRM.dds");
	render.SetTexture(device, gcl,			"EarthNormalMap",			L"../Textures/SolarSystem/Earth_Normal.dds");
	render.SetTexture(device, gcl,			"MoonNormalMap",			L"../Textures/SolarSystem/Moon_NRM.dds");
	render.SetTexture(device, gcl,			"MarsNormalMap",			L"../Textures/SolarSystem/Mars_NRM.dds");
	render.SetTexture(device, gcl,			"NeutralNormalMap",			L"../Textures/SolarSystem/neutral.dds");

	render.SetTexture(device, gcl,			"dds",						L"../Textures/SolarSystem/dds2.dds");

	render.SetTexture(device, gcl,			"debugDiffuseMap",			L"../Textures/tile.dds");
	render.SetTexture(device, gcl,			"debugNormalMap",			L"../Textures/tile_nmap.dds");
}

void InitializeMaterials(Render& render)
{
	render.SetMaterial("UniverseMat",	"UniverseDiffuseMap",		"NeutralNormalMap",			1.0f, 1.0f, 1.0f, 1.0f,			0.0f, 0.0f, 0.0f,			0.0f);
	render.SetMaterial("SunMat",		"SunDiffuseMap",			"NeutralNormalMap",			1.0f, 1.0f, 1.0f, 1.0f,			0.1f, 0.1f, 0.1f,			0.5f);
	render.SetMaterial("MercuryMat",	"MercuryDiffuseMap",		"NeutralNormalMap",			1.0f, 1.0f, 1.0f, 1.0f,			0.1f, 0.1f, 0.1f,			0.5f);
	render.SetMaterial("VenusMat",		"VenusDiffuseMap",			"NeutralNormalMap",			1.0f, 1.0f, 1.0f, 1.0f,			0.1f, 0.1f, 0.1f,			0.5f);
	render.SetMaterial("EarthMat",		"EarthDiffuseMap",			"NeutralNormalMap",			1.0f, 1.0f, 1.0f, 1.0f,			0.1f, 0.1f, 0.1f,			0.5f);
	render.SetMaterial("MarsMat",		"MarsDiffuseMap",			"NeutralNormalMap",			1.0f, 1.0f, 1.0f, 1.0f,			0.1f, 0.1f, 0.1f,			0.5f);
	render.SetMaterial("JupiterMat",	"JupiterDiffuseMap",		"NeutralNormalMap",			1.0f, 1.0f, 1.0f, 1.0f,			0.1f, 0.1f, 0.1f,			0.5f);
	render.SetMaterial("SaturnMat",		"SaturnDiffuseMap",			"NeutralNormalMap",			1.0f, 1.0f, 1.0f, 1.0f,			0.1f, 0.1f, 0.1f,			0.5f);
	render.SetMaterial("UranusMat",		"UranusDiffuseMap",			"NeutralNormalMap",			1.0f, 1.0f, 1.0f, 1.0f,			0.1f, 0.1f, 0.1f,			0.5f);
	render.SetMaterial("NeptuneMat",	"NeptuneDiffuseMap",		"NeutralNormalMap",			1.0f, 1.0f, 1.0f, 1.0f,			0.1f, 0.1f, 0.1f,			0.5f);
	render.SetMaterial("SpriteMat",		"sprite",					"",							1.0f, 1.0f, 1.0f, 1.0f,			0.1f, 0.1f, 0.1f,			0.5f);
	render.SetMaterial("debug",			"ds",						"dds",						1.0f, 1.0f, 1.0f, 1.0f,			0.1f, 0.1f, 0.1f,			0.5f);
}

void InitializeGameObjects(Scene& scene)
{
	// Подготовка камеры
	Camera* camera = new Camera();
	camera->SetPosition(-20.0f, 15.0f, 0.0f);
	scene.SetMainCamera(camera);

	// Добавление объектов на сцену
	scene.AddGameObject(new Universe("Universe",	"SkysphereGeo",		"UniverseMat"));
	//scene.AddGameObject(new Planet("Sun",			"SphereGeo",		"SunMat",			1.0f,					0.0f,		0.0f));
	scene.AddGameObject(new Planet("Mercury",		"SphereGeo",		"MercuryMat",		0.3824082784571966f,	0.387f,		87.97f));
	scene.AddGameObject(new Planet("Venus",			"SphereGeo",		"VenusMat",			0.9488867983693948f,	0.723f,		224.7f));
	GameObject* go = new Planet("Earth",			"SphereGeo",		"EarthMat", 1.0f, 1.0f, 365.25f);
	scene.AddGameObject(go);
	//scene.AddGameObject(new Moon("Moon",			"SphereGeo",		"NeptuneMat", 0.3f, go, 0.2f, 1.0f));
	scene.AddGameObject(new Planet("Mars",			"SphereGeo",		"MarsMat",			0.5468798996550643f,	1.52f,		686.94f));
	scene.AddGameObject(new Planet("Jupiter",		"SphereGeo",		"JupiterMat",		11.17905299466918f,		5.20f,		4332.59f));
	scene.AddGameObject(new Planet("Saturn",		"SphereGeo",		"SaturnMat",		9.423016619629978f,		9.54f,		10759.0f));
	scene.AddGameObject(new Planet("Uranus",		"SphereGeo",		"UranusMat",		4.154907494512386f,		19.19f,		30688.5f));
	scene.AddGameObject(new Planet("Neptune",		"SphereGeo",		"NeptuneMat",		3.880526810912512f,		30.07f,		60182.0f));


	Katamari* player = new Katamari("Player", "SphereGeo", "SunMat", 1.0f, camera);
	scene.AddGameObject(player);

	scene.AddGameObject(new Element("P1", "SphereGeo", "EarthMat", 1.0f, player, 2, 0, 4));
	scene.AddGameObject(new Element("P2", "SphereGeo", "EarthMat", 1.0f, player, 2, 0, 2));
	//scene.AddGameObject(new Element("P3", "SphereGeo", "EarthMat", 1.0f, player, 2, 0, 0));
	scene.AddGameObject(new Element("P4", "SphereGeo", "EarthMat", 1.0f, player, 2, 0, -2));

	scene.AddGameObject(new Element("P05", "SphereGeo", "EarthMat", 0.9f, player, 6, 0, -8));
	scene.AddGameObject(new Element("P06", "SphereGeo", "EarthMat", 0.9f, player, 1, 0, 1));
	scene.AddGameObject(new Element("P07", "SphereGeo", "EarthMat", 0.9f, player, 8, 0, 5));
	scene.AddGameObject(new Element("P08", "SphereGeo", "EarthMat", 0.8f, player, 3, 0, 4));
	scene.AddGameObject(new Element("P09", "SphereGeo", "EarthMat", 0.8f, player, 3, 0, 3));
	scene.AddGameObject(new Element("P10", "SphereGeo", "EarthMat", 0.8f, player, 6, 0, 6));
	scene.AddGameObject(new Element("P11", "SphereGeo", "EarthMat", 0.7f, player, 6, 0, -1));
	scene.AddGameObject(new Element("P12", "SphereGeo", "EarthMat", 0.7f, player, 4, 0, -4));
	scene.AddGameObject(new Element("P13", "SphereGeo", "EarthMat", 0.7f, player, 7, 0, -2));
	scene.AddGameObject(new Element("P14", "SphereGeo", "EarthMat", 0.6f, player, 3, 0, -4));
	scene.AddGameObject(new Element("P15", "SphereGeo", "EarthMat", 0.6f, player, 2, 0, -7));
	scene.AddGameObject(new Element("P16", "SphereGeo", "EarthMat", 0.6f, player, 0, 0, -4));

	scene.AddGameObject(new Platform("Platform",	"GridGeo",			"debug",			1.0f));
}

void MyEngine::InitializeShaders()
{
	const D3D_SHADER_MACRO alphaTestDefines[] =
	{
		"ALPHA_TEST", "1",
		NULL, NULL
	};

	render.CreateShader("defaultVS",				L"Default.hlsl",		"VS",		"vs_5_1");
	render.CreateShader("defaultPS",				L"Default.hlsl",		"PS",		"ps_5_1");

	render.CreateShader("shadowVS",					L"Shadows.hlsl",		"VS",		"vs_5_1");
	render.CreateShader("shadowOpaquePS",			L"Shadows.hlsl",		"PS",		"ps_5_1");
	render.CreateShader("shadowAlphaTestedPS",		L"Shadows.hlsl",		"PS",		"ps_5_1",		alphaTestDefines);

	render.CreateShader("debugVS",					L"ShadowDebug.hlsl",	"VS",		"vs_5_1");
	render.CreateShader("debugPS",					L"ShadowDebug.hlsl",	"PS",		"ps_5_1");

	render.CreateShader("drawNormalsVS",			L"DrawNormals.hlsl",	"VS",		"vs_5_1");
	render.CreateShader("drawNormalsPS",			L"DrawNormals.hlsl",	"PS",		"ps_5_1");

	render.CreateShader("ssaoVS",					L"Ssao.hlsl",			"VS",		"vs_5_1");
	render.CreateShader("ssaoPS",					L"Ssao.hlsl",			"PS",		"ps_5_1");

	render.CreateShader("ssaoBlurVS",				L"SsaoBlur.hlsl",		"VS",		"vs_5_1");
	render.CreateShader("ssaoBlurPS",				L"SsaoBlur.hlsl",		"PS",		"ps_5_1");

	render.CreateShader("skyVS",					L"Sky.hlsl",			"VS",		"vs_5_1");
	render.CreateShader("skyPS",					L"Sky.hlsl",			"PS",		"ps_5_1");

	render.CreateShader("shadowQuadPS",				L"ShadowQuad.hlsl",		"PS",		"ps_5_1");
	render.CreateShader("shadowQuadVS",				L"ShadowQuad.hlsl",		"VS",		"vs_5_1");

	render.CreateShader("ssaoQuadVS",				L"SsaoQuad.hlsl",		"VS",		"vs_5_1");
	render.CreateShader("ssaoQuadPS",				L"SsaoQuad.hlsl",		"PS",		"ps_5_1");

	render.CreateShader("billboardVS",				L"TreeSprite.hlsl",		"VS",		"vs_5_1");
	render.CreateShader("billboardGS",				L"TreeSprite.hlsl",		"GS",		"gs_5_1");
	render.CreateShader("billboardPS",				L"TreeSprite.hlsl",		"PS",		"ps_5_1");

	mTreeSpriteInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "SIZE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
}
#pragma endregion