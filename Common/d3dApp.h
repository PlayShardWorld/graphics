//***************************************************************************************
// d3dApp.h by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

#pragma once

#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include "d3dUtil.h"
#include "GameTimer.h"
#include "GameInput.h"

// Линковка необходимых d3d12 библиотек.
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")

class D3DApp
{
protected:

    D3DApp(HINSTANCE hInstance);
    D3DApp(const D3DApp& rhs) = delete;
    D3DApp& operator=(const D3DApp& rhs) = delete;
    virtual ~D3DApp();

public:

    static D3DApp* GetApp();
    
	HINSTANCE AppInst()const;
	HWND      MainWnd()const;
	float     AspectRatio()const;

    bool Get4xMsaaState()const;
    void Set4xMsaaState(bool value);

	int Run();
 
    virtual bool Initialize();
    virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

protected:
    virtual void CreateRtvAndDsvDescriptorHeaps();  //
	virtual void OnResize();                        // Действие при изменении размера окна

    virtual void GameLoop(const GameTimer& gt) = 0;
    virtual void Draw(const GameTimer& gt) = 0;

	// Перегрузки для обработки событий мыши
	virtual void OnMouseDown(WPARAM btnState, int x, int y) { }
	virtual void OnMouseUp(WPARAM btnState, int x, int y)   { }
	virtual void OnMouseMove(WPARAM btnState, int x, int y) { }

protected:

    // MY
    bool InitializeDirect3D();

	bool InitMainWindow();
	//bool InitDirect3D();
	//void CreateCommandObjects();
    void CreateSwapChain();

	void FlushCommandQueue();

	ID3D12Resource* CurrentBackBuffer()const;
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView()const;
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView()const;

	void CalculateFrameStats();

    void LogAdapters();
    void LogAdapterOutputs(IDXGIAdapter* adapter);
    void LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format);

protected:
    GameTimer _GlobalTimer;

    static D3DApp* mApp;

    HINSTANCE mhAppInst = nullptr;      // application instance handle
    HWND      mhMainWnd = nullptr;      // main window handle
	bool      mAppPaused = false;       // is the application paused?
	bool      mMinimized = false;       // is the application minimized?
	bool      mMaximized = false;       // is the application maximized?
	bool      mResizing = false;        // are the resize bars being dragged?
    bool      mFullscreenState = false; // Флаг полноэкранного режима

	// Для включения 4X MSAA необходимо установить значение 'true'
    bool      m4xMsaaState = false;    // Флаг 4X MSAA
    UINT      m4xMsaaQuality = 0;      // Уровень качества 4X MSAA

#pragma region MyVariables
    // Основное
    Microsoft::WRL::ComPtr<IDXGIFactory4> _dxgiFactory;                     // Фабрика ресурсов
    Microsoft::WRL::ComPtr<ID3D12Device> _Device;                           // Устрйоство

    // Цепочка обмена
    Microsoft::WRL::ComPtr<IDXGISwapChain> _SwapChain;                      // Цепочка обмена
    static const int _SwapChainBufferCount = 2;                             // Количество кадров в цепочке обмена
    int _CurrentBackBufferIndex = 0;                                        // Текущий индекс заднего буфера

    // Команды отрисовки
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> _CommandQueue;               // Очередь команд
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> _CommandAllocator;       // Аллокатор команд
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> _GraphicsCommandList; // Список команд

    // Барьер
    Microsoft::WRL::ComPtr<ID3D12Fence> _Fence;                             // Синхронизация CPU/GPU с помощью барьера
    UINT64 _CurrentFenceIndex = 0;                                          // Текущий индекс барьера

    // Дескрипторы куч
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _DescriptorHeapRTV;        // Дескриптор кучи RTV
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _DescriptorHeapDSV;        // Дескриптор кучи DSV

    // Размеры дескрипторов
    UINT _DescriptorSizeRTV = 0;                                            // Размер дескриптора RTV
    UINT _DescriptorSizeDSV = 0;                                            // Размер дескриптора DSV
    UINT _DescriptorSizeCSU = 0;                                            // Размер дескрипторов CBV, SRV, UAV

    // Задний буфер и буфер глубины/трафарета
    Microsoft::WRL::ComPtr<ID3D12Resource> _SwapChainBuffer[_SwapChainBufferCount];//
    Microsoft::WRL::ComPtr<ID3D12Resource> _DepthStencilBuffer;//

    // 
    D3D12_VIEWPORT _ScreenViewport;
    D3D12_RECT _ScissorRect;

    // Derived class should set these in derived constructor to customize starting values.
    std::wstring _MainWndCaption = L"WinAPI DirectX12";

    DXGI_FORMAT _BackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT _DepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    int _ClientWidth = 800;
    int _ClientHeight = 600;
#pragma endregion

	D3D_DRIVER_TYPE md3dDriverType = D3D_DRIVER_TYPE_HARDWARE;//TODO

};

