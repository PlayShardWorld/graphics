//***************************************************************************************
// d3dApp.cpp by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

#include "d3dApp.h"
#include <WindowsX.h>

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;

LRESULT CALLBACK
MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// Forward hwnd on because we can get messages (e.g., WM_CREATE)
	// before CreateWindow returns, and thus before mhMainWnd is valid.
    return D3DApp::GetApp()->MsgProc(hwnd, msg, wParam, lParam);
}

D3DApp* D3DApp::mApp = nullptr;

// ��������� ��������� �� ����������
D3DApp* D3DApp::GetApp()
{
    return mApp;
}

// ����������� �� ���������
D3DApp::D3DApp(HINSTANCE hInstance)
:	mhAppInst(hInstance)
{
	// ��������, ��� ���������� �������� �� ��������
    assert(mApp == nullptr);
    mApp = this;
}

// ���������� �� ���������
D3DApp::~D3DApp()
{
	// ���� ������� ���������� Direct3D - ���������� ������ ������
	if (_Device != nullptr)
		FlushCommandQueue();
}

// ��������� ������ �� ����������
HINSTANCE D3DApp::AppInst()const
{
	return mhAppInst;
}

// ��������� ������ �� ������� ����
HWND D3DApp::MainWnd()const
{
	return mhMainWnd;
}

// ��������� ������������ ����������� ������ ������
float D3DApp::AspectRatio()const
{
	return static_cast<float>(_ClientWidth) / _ClientHeight;
}

// ��������� ��������� 4X MSAA
bool D3DApp::Get4xMsaaState() const
{
    return m4xMsaaState;
}

// ��������� ��������� 4X MSAA
void D3DApp::Set4xMsaaState(bool value)
{
    if (m4xMsaaState != value)
    {
        m4xMsaaState = value;

		// ������������ Swap Chain � ������� ��� ����� �������� ���������������
        CreateSwapChain();
        OnResize();
    }
}

// ����������� Game Loop � ����������
int D3DApp::Run()
{
	MSG msg = {0};
 
	// ����� �������
	_GlobalTimer.Reset();

	while (msg.message != WM_QUIT)
	{
		// ��������� ��������� Windows, ���� ��� ����
		if(PeekMessage( &msg, 0, 0, 0, PM_REMOVE ))
		{
            TranslateMessage( &msg );
            DispatchMessage( &msg );
		}
		// ����� ��������� ���������
		else
        {	
			_GlobalTimer.Tick();

			if( !mAppPaused )
			{
				CalculateFrameStats();
				GameLoop(_GlobalTimer);
                Draw(_GlobalTimer);
			}
			else
			{
				Sleep(100);
			}
        }
    }

	return (int)msg.wParam;
}

// ������������� ���������� �� ���������
bool D3DApp::Initialize()
{
	// ������������� �������� ����
	if(!InitMainWindow())
		return false;

	// ������������� Direct3D
	if(!InitializeDirect3D())
		return false;

    // ��������� �������� ��� ��������� �������� ����
    OnResize();

	return true;
}

void D3DApp::OnResize()
{
	// ��������, ��� ����������, ������� ������ � ��������� ��������� �������
	assert(_Device);
	assert(_SwapChain);
    assert(_CommandAllocator);

	// ������� ������� ������ ����� ���������� ����� ��������
	FlushCommandQueue();

#pragma region Reset : Graphics Command List, Swap Chain Buffer, Depth/Stencil Buffer
	ThrowIfFailed(_GraphicsCommandList->Reset(_CommandAllocator.Get(), nullptr));

	// ������������ ��������, ������� ����� ����� ��������������
	for (int i = 0; i < _SwapChainBufferCount; ++i)
		_SwapChainBuffer[i].Reset();
	_DepthStencilBuffer.Reset();
#pragma endregion

#pragma region Resize the Swap Chain Buffer
	// ���������� ������� ������ ������� ������
	ThrowIfFailed(_SwapChain->ResizeBuffers(
		_SwapChainBufferCount,
		_ClientWidth, _ClientHeight,
		_BackBufferFormat,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

	_CurrentBackBufferIndex = 0;
#pragma endregion
 
#pragma region Create frame resources (RTV for each frame)
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(_DescriptorHeapRTV->GetCPUDescriptorHandleForHeapStart());
	for (UINT i = 0; i < _SwapChainBufferCount; i++)
	{
		ThrowIfFailed(_SwapChain->GetBuffer(i, IID_PPV_ARGS(&_SwapChainBuffer[i])));
		_Device->CreateRenderTargetView(_SwapChainBuffer[i].Get(), nullptr, rtvHeapHandle);
		rtvHeapHandle.Offset(1, _DescriptorSizeRTV);
	}
#pragma endregion



	/// ��� 8. �������� ������ �������/��������� � �������������

	// �������� ������ �������/���������
    D3D12_RESOURCE_DESC depthStencilDesc;
    depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthStencilDesc.Alignment = 0;
    depthStencilDesc.Width = _ClientWidth;
    depthStencilDesc.Height = _ClientHeight;
    depthStencilDesc.DepthOrArraySize = 1;
    depthStencilDesc.MipLevels = 1;

	// Correction 11/12/2016: SSAO chapter requires an SRV to the depth buffer to read from 
	// the depth buffer.  Therefore, because we need to create two views to the same resource:
	//   1. SRV format: DXGI_FORMAT_R24_UNORM_X8_TYPELESS
	//   2. DSV Format: DXGI_FORMAT_D24_UNORM_S8_UINT
	// we need to create the depth buffer resource with a typeless format.  
	depthStencilDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;

    depthStencilDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    depthStencilDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE optClear;
    optClear.Format = _DepthStencilFormat;
    optClear.DepthStencil.Depth = 1.0f;
    optClear.DepthStencil.Stencil = 0;
    ThrowIfFailed(_Device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
        &depthStencilDesc,
		D3D12_RESOURCE_STATE_COMMON,
        &optClear,
        IID_PPV_ARGS(_DepthStencilBuffer.GetAddressOf())));






    // Create descriptor to mip level 0 of entire resource using the format of the resource.
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Format = _DepthStencilFormat;
	dsvDesc.Texture2D.MipSlice = 0;
    _Device->CreateDepthStencilView(_DepthStencilBuffer.Get(), &dsvDesc, DepthStencilView());

    // Transition the resource from its initial state to be used as a depth buffer.
	_GraphicsCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_DepthStencilBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));
	
	/////////////////////////////////////////

    // Execute the resize commands.
    ThrowIfFailed(_GraphicsCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { _GraphicsCommandList.Get() };
    _CommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until resize is complete.
	FlushCommandQueue();

#pragma region Update the Screen Viewport and Scissor Rectangle
	// ��������� Screen Viewport ��� ����������� ������� ������� ���������
	_ScreenViewport.TopLeftX = 0;
	_ScreenViewport.TopLeftY = 0;
	_ScreenViewport.Width = static_cast<float>(_ClientWidth);
	_ScreenViewport.Height = static_cast<float>(_ClientHeight);
	_ScreenViewport.MinDepth = 0.0f;
	_ScreenViewport.MaxDepth = 1.0f;

	/// ��������� ������� �������
	_ScissorRect = { 0, 0, _ClientWidth, _ClientHeight };
#pragma endregion
}

// ��������� ������� ����������
LRESULT D3DApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch( msg )
	{
	// WM_ACTIVATE is sent when the window is activated or deactivated.  
	// We pause the game when the window is deactivated and unpause it 
	// when it becomes active.  
	case WM_ACTIVATE:
		if( LOWORD(wParam) == WA_INACTIVE )
		{
			mAppPaused = true;
			_GlobalTimer.Stop();
		}
		else
		{
			mAppPaused = false;
			_GlobalTimer.Start();
		}
		return 0;

	// WM_SIZE is sent when the user resizes the window.  
	case WM_SIZE:
		// Save the new client area dimensions.
		_ClientWidth  = LOWORD(lParam);
		_ClientHeight = HIWORD(lParam);
		if( _Device )
		{
			if( wParam == SIZE_MINIMIZED )
			{
				mAppPaused = true;
				mMinimized = true;
				mMaximized = false;
			}
			else if( wParam == SIZE_MAXIMIZED )
			{
				mAppPaused = false;
				mMinimized = false;
				mMaximized = true;
				OnResize();
			}
			else if( wParam == SIZE_RESTORED )
			{
				
				// Restoring from minimized state?
				if( mMinimized )
				{
					mAppPaused = false;
					mMinimized = false;
					OnResize();
				}

				// Restoring from maximized state?
				else if( mMaximized )
				{
					mAppPaused = false;
					mMaximized = false;
					OnResize();
				}
				else if( mResizing )
				{
					// If user is dragging the resize bars, we do not resize 
					// the buffers here because as the user continuously 
					// drags the resize bars, a stream of WM_SIZE messages are
					// sent to the window, and it would be pointless (and slow)
					// to resize for each WM_SIZE message received from dragging
					// the resize bars.  So instead, we reset after the user is 
					// done resizing the window and releases the resize bars, which 
					// sends a WM_EXITSIZEMOVE message.
				}
				else // API call such as SetWindowPos or mSwapChain->SetFullscreenState.
				{
					OnResize();
				}
			}
		}
		return 0;

	// WM_EXITSIZEMOVE is sent when the user grabs the resize bars.
	case WM_ENTERSIZEMOVE:
		mAppPaused = true;
		mResizing  = true;
		_GlobalTimer.Stop();
		return 0;

	// WM_EXITSIZEMOVE is sent when the user releases the resize bars.
	// Here we reset everything based on the new window dimensions.
	case WM_EXITSIZEMOVE:
		mAppPaused = false;
		mResizing  = false;
		_GlobalTimer.Start();
		OnResize();
		return 0;
 
	// WM_DESTROY is sent when the window is being destroyed.
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	// The WM_MENUCHAR message is sent when a menu is active and the user presses 
	// a key that does not correspond to any mnemonic or accelerator key. 
	case WM_MENUCHAR:
        // Don't beep when we alt-enter.
        return MAKELRESULT(0, MNC_CLOSE);

	// Catch this message so to prevent the window from becoming too small.
	case WM_GETMINMAXINFO:
		((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
		((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200; 
		return 0;

	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_MOUSEMOVE:
		OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
    case WM_KEYUP:
        if(wParam == VK_ESCAPE)
        {
            PostQuitMessage(0);
        }
        //else if((int)wParam == VK_F2)
        //    Set4xMsaaState(!m4xMsaaState);

        return 0;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ������������� �������� ����
bool D3DApp::InitMainWindow()
{
	WNDCLASS wc;
	wc.style         = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc   = MainWndProc; 
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = mhAppInst;
	wc.hIcon         = LoadIcon(0, IDI_APPLICATION);
	wc.hCursor       = LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	wc.lpszMenuName  = 0;
	wc.lpszClassName = L"MainWnd";

	if( !RegisterClass(&wc) )
	{
		MessageBox(0, L"RegisterClass Failed.", 0, 0);
		return false;
	}

	// Compute window rectangle dimensions based on requested client area dimensions.
	RECT R = { 0, 0, _ClientWidth, _ClientHeight };
    AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
	int width  = R.right - R.left;
	int height = R.bottom - R.top;

	mhMainWnd = CreateWindow(L"MainWnd", _MainWndCaption.c_str(), 
		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, 0, 0, mhAppInst, 0); 
	if( !mhMainWnd )
	{
		MessageBox(0, L"CreateWindow Failed.", 0, 0);
		return false;
	}

	ShowWindow(mhMainWnd, SW_SHOW);
	UpdateWindow(mhMainWnd);

	return true;
}

// ������������� �������� Direct3D
bool D3DApp::InitializeDirect3D()
{
	UINT dxgiFactoryFlags = 0;

#if defined(DEBUG) || defined(_DEBUG) 
	// ��������� ���� �������
	{
		ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();

			// ��������� ��������������� ���� �������
			dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
		}
	}
#endif

	// �������� ������� ��������
	ThrowIfFailed(CreateDXGIFactory2(
		dxgiFactoryFlags,
		IID_PPV_ARGS(&_dxgiFactory)
	));

#pragma region Create the Device
	HRESULT hardwareResult = D3D12CreateDevice(
		nullptr,					// ���������� �������� �������
		D3D_FEATURE_LEVEL_12_1,		// ���������� DirectX 12.1
		IID_PPV_ARGS(&_Device));

	// � ������ ������� ������������ � ������������� WARP ����������
	if (FAILED(hardwareResult))
	{
		ComPtr<IDXGIAdapter> warpAdapter;
		ThrowIfFailed(_dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

		ThrowIfFailed(D3D12CreateDevice(
			warpAdapter.Get(),
			D3D_FEATURE_LEVEL_12_1,
			IID_PPV_ARGS(&_Device)
		));
	}
#pragma endregion

#pragma region Create the Command Queue
	// �������� ������� ������
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

	// �������� ������� ������
	ThrowIfFailed(_Device->CreateCommandQueue(
		&queueDesc,
		IID_PPV_ARGS(&_CommandQueue)
	));
#pragma endregion

#pragma region Create a Command Allocator
	ThrowIfFailed(_Device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(&_CommandAllocator)
	));
#pragma endregion

#pragma region Create a Graphics Command List
	ThrowIfFailed(_Device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		_CommandAllocator.Get(),
		nullptr,								// PSO
		IID_PPV_ARGS(&_GraphicsCommandList)
	));

	ThrowIfFailed(_GraphicsCommandList->Close());
#pragma endregion

#pragma region Create a Fence
	ThrowIfFailed(_Device->CreateFence(
		0,
		D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&_Fence)
	));
#pragma endregion

#pragma region Create the Swap Chain
	// �������� ������� ������
	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	swapChainDesc.BufferCount = _SwapChainBufferCount;				// ���������� ������� ������������ � Swap Chain
	swapChainDesc.OutputWindow = mhMainWnd;							// � ����� ���� ����� ����������� ���������
	swapChainDesc.Windowed = true;									// FALSE - ������������� �����, TRUE - �������
	swapChainDesc.BufferDesc.Width = _ClientWidth;
	swapChainDesc.BufferDesc.Height = _ClientHeight;
	swapChainDesc.BufferDesc.RefreshRate.Numerator = 240;
	swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	swapChainDesc.BufferDesc.Format = _BackBufferFormat;
	swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	swapChainDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;							// ������� 4X MSAA
	swapChainDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;		// �������� 4X MSAA

	// �������� ������� ������
	ThrowIfFailed(_dxgiFactory->CreateSwapChain(
		_CommandQueue.Get(),						// ������� ������ ����� ������� ������, ����� ��� ����� ������������� �������� �
		&swapChainDesc,
		_SwapChain.GetAddressOf()));
#pragma endregion

	CreateRtvAndDsvDescriptorHeaps();

#pragma region Get size of Heap Descriptors
	_DescriptorSizeRTV = _Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	_DescriptorSizeDSV = _Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	_DescriptorSizeCSU = _Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
#pragma endregion
}

void D3DApp::CreateRtvAndDsvDescriptorHeaps()
{
#pragma region Create a RTV (Render Target View) Decriptor Heap
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = _SwapChainBufferCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	ThrowIfFailed(_Device->CreateDescriptorHeap(
		&rtvHeapDesc,
		IID_PPV_ARGS(_DescriptorHeapRTV.GetAddressOf())
	));
#pragma endregion

#pragma region Create a DSV (Depth/Stencil View) Decriptor Heap
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 2;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	ThrowIfFailed(_Device->CreateDescriptorHeap(
		&dsvHeapDesc,
		IID_PPV_ARGS(_DescriptorHeapDSV.GetAddressOf())
	));
#pragma endregion
}

//// ������������� Direct3D (���� 1-6 �������������)
//bool D3DApp::InitDirect3D()
//{
//#if defined(DEBUG) || defined(_DEBUG) 
//	// ��������� ������� D3D12
//{
//	ComPtr<ID3D12Debug> debugController;
//	ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
//	debugController->EnableDebugLayer();
//}
//#endif
//
//	/// ��� 1. �������� ����������� ����������
//
//	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&mdxgiFactory)));
//	
//	HRESULT hardwareResult = D3D12CreateDevice(
//		nullptr,					// ���������� �������� �������
//		D3D_FEATURE_LEVEL_11_0,		// ���������� DirectX 11.0
//		IID_PPV_ARGS(&md3dDevice));	// COM ID ������������ ����������
//
//	// � ������ ������� ������������ � ������������� WARP ����������
//	if (FAILED(hardwareResult))
//	{
//		ComPtr<IDXGIAdapter> pWarpAdapter;
//		ThrowIfFailed(mdxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&pWarpAdapter)));
//
//		ThrowIfFailed(D3D12CreateDevice(
//			pWarpAdapter.Get(),
//			D3D_FEATURE_LEVEL_11_0,
//			IID_PPV_ARGS(&md3dDevice)));
//	}
//
//	/// ��� 2. �������� ������� Fence � ������ �������� ������������
//
//	// ������� ������� ������ Fence
//	ThrowIfFailed(md3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE,
//		IID_PPV_ARGS(&mFence)));
//
//	// TODO ��������� �������� ������������
//	mRtvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
//	mDsvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
//	mCbvSrvUavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
//
//	/// TODO ��� 3. �������� ��������� 4X MSAA
//
//	// ��������� �������������� �������� 4X MSAA  ��� ������ ������� ������
//	// ��� ����������, �������������� Direct3D 11 ������������ 4X MSAA
//	// ��� ����� ������ ��������� ��������� ������� �������� 4X MSAA
//
//	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
//	msQualityLevels.Format = _BackBufferFormat;
//	msQualityLevels.SampleCount = 4;
//	msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
//	msQualityLevels.NumQualityLevels = 0;
//	ThrowIfFailed(md3dDevice->CheckFeatureSupport(
//		D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
//		&msQualityLevels,
//		sizeof(msQualityLevels)));
//
//    m4xMsaaQuality = msQualityLevels.NumQualityLevels;
//	assert(m4xMsaaQuality > 0 && "Unexpected MSAA quality level.");
//	
//#ifdef _DEBUG
//    LogAdapters();
//#endif
//
//	// ����� �������� ���� 4
//	CreateCommandObjects();
//
//	// ����� �������� ���� 5
//    CreateSwapChain();
//
//	// ����� �������� ���� 6
//    CreateRtvAndDsvDescriptorHeaps();
//
//	return true;
//}

//#pragma region OLD
//// ��� 4. �������� Command Queue, Command Allocator � Command List
//void D3DApp::CreateCommandObjects()
//{
//	// ��������� ��������� ������������� Command Queue
//	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
//	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
//	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
//
//	// �������� ������� Command Queue
//	ThrowIfFailed(md3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue)));
//
//	// �������� ������� Command Allocator
//	ThrowIfFailed(md3dDevice->CreateCommandAllocator(
//		D3D12_COMMAND_LIST_TYPE_DIRECT,
//		IID_PPV_ARGS(mDirectCmdListAlloc.GetAddressOf())));
//
//	// �������� ������� Command List
//	ThrowIfFailed(md3dDevice->CreateCommandList(
//		0,
//		D3D12_COMMAND_LIST_TYPE_DIRECT,
//		mDirectCmdListAlloc.Get(), // ��������� Command Allocator
//		nullptr,                   // ������������� PipelineStateObject
//		IID_PPV_ARGS(mCommandList.GetAddressOf())));
//
//	// ������ � �������� ���������. ��� ������� � ���, ��� ��� ������ ���������
//	// � ������ ������ �� ������� ���, � ��� ���������� ������� ����� ������� Reset
//	mCommandList->Close();
//}
//
// ��� 5. �������� Swap Chain (������� �����������)
void D3DApp::CreateSwapChain()
{
	// ����� ���������� SwapChain ����� �������������
	_SwapChain.Reset();

	// ��������� ���������������� ������ ��� �������� SwapChain
	DXGI_SWAP_CHAIN_DESC sd;
	sd.BufferDesc.Width = _ClientWidth;
	sd.BufferDesc.Height = _ClientHeight;
	sd.BufferDesc.RefreshRate.Numerator = 240;								// TODO ����� ��� ��� ���������� ���������� ������ � �������
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferDesc.Format = _BackBufferFormat;
	sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	sd.SampleDesc.Count = m4xMsaaState ? 4 : 1;								// ������� 4X MSAA
	sd.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;		// �������� 4X MSAA
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = _SwapChainBufferCount;									// ���������� ������� ������������ � Swap Chain
	sd.OutputWindow = mhMainWnd;											// � ����� ���� ����� ����������� ���������
	sd.Windowed = true;														// FALSE - ������������� �����, TRUE - �������
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	// ������� Swap Chain (���������� Command Queue)
	ThrowIfFailed(_dxgiFactory->CreateSwapChain(
		_CommandQueue.Get(),
		&sd,
		_SwapChain.GetAddressOf()));
}
//
//// ��� 6. �������� ������������ ���
//void D3DApp::CreateRtvAndDsvDescriptorHeaps()
//{
//	// �������� ����������� ���� RTV (Render Target View)
//	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
//	rtvHeapDesc.NumDescriptors = SwapChainBufferCount;
//	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
//	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
//	rtvHeapDesc.NodeMask = 0;
//	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
//		&rtvHeapDesc, IID_PPV_ARGS(mRtvHeap.GetAddressOf())));
//
//	// �������� ����������� ���� DSV (Depth/Stencil View)
//	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
//	dsvHeapDesc.NumDescriptors = 1;
//	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
//	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
//	dsvHeapDesc.NodeMask = 0;
//	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
//		&dsvHeapDesc, IID_PPV_ARGS(mDsvHeap.GetAddressOf())));
//}
//#pragma endregion

void D3DApp::FlushCommandQueue()
{
	// Advance the fence value to mark commands up to this fence point.
	_CurrentFenceIndex++;

    // Add an instruction to the command queue to set a new fence point.  Because we 
	// are on the GPU timeline, the new fence point won't be set until the GPU finishes
	// processing all the commands prior to this Signal().
    ThrowIfFailed(_CommandQueue->Signal(_Fence.Get(), _CurrentFenceIndex));

	// Wait until the GPU has completed commands up to this fence point.
    if(_Fence->GetCompletedValue() < _CurrentFenceIndex)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);

        // Fire event when GPU hits current fence.  
        ThrowIfFailed(_Fence->SetEventOnCompletion(_CurrentFenceIndex, eventHandle));

        // Wait until the GPU hits current fence event is fired.
		WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
	}
}

ID3D12Resource* D3DApp::CurrentBackBuffer() const
{
	return _SwapChainBuffer[_CurrentBackBufferIndex].Get();
}

// ��������� View �������� ������� ������
D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::CurrentBackBufferView() const
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(
		_DescriptorHeapRTV->GetCPUDescriptorHandleForHeapStart(),
		_CurrentBackBufferIndex,
		_DescriptorSizeRTV);
}

// ��������� View ������ Depth/Stencil
D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::DepthStencilView() const
{
	return _DescriptorHeapDSV->GetCPUDescriptorHandleForHeapStart();
}

// ������ ���������� �� ����� � ������ ���������� � ��������� ����
void D3DApp::CalculateFrameStats()
{
	// Code computes the average frames per second, and also the 
	// average time it takes to render one frame.  These stats 
	// are appended to the window caption bar.
    
	static int frameCnt = 0;
	static float timeElapsed = 0.0f;

	frameCnt++;

	// Compute averages over one second period.
	if( (_GlobalTimer.TotalTime() - timeElapsed) >= 1.0f )
	{
		float fps = (float)frameCnt; // fps = frameCnt / 1
		float mspf = 1000.0f / fps;

        wstring fpsStr = to_wstring(fps);
        wstring mspfStr = to_wstring(mspf);

        wstring windowText = _MainWndCaption +
            L"     \tFPS: " + fpsStr +
            L"     \tms: " + mspfStr;

        SetWindowText(mhMainWnd, windowText.c_str());
		
		// Reset for next average.
		frameCnt = 0;
		timeElapsed += 1.0f;
	}
}

#pragma region Log ?
void D3DApp::LogAdapters()
{
	UINT i = 0;
	IDXGIAdapter* adapter = nullptr;
	std::vector<IDXGIAdapter*> adapterList;
	while (_dxgiFactory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_ADAPTER_DESC desc;
		adapter->GetDesc(&desc);

		std::wstring text = L"***Adapter: ";
		text += desc.Description;
		text += L"\n";

		OutputDebugString(text.c_str());

		adapterList.push_back(adapter);

		++i;
	}

	for (size_t i = 0; i < adapterList.size(); ++i)
	{
		LogAdapterOutputs(adapterList[i]);
		ReleaseCom(adapterList[i]);
	}
}

void D3DApp::LogAdapterOutputs(IDXGIAdapter* adapter)
{
	UINT i = 0;
	IDXGIOutput* output = nullptr;
	while (adapter->EnumOutputs(i, &output) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_OUTPUT_DESC desc;
		output->GetDesc(&desc);

		std::wstring text = L"***Output: ";
		text += desc.DeviceName;
		text += L"\n";
		OutputDebugString(text.c_str());

		LogOutputDisplayModes(output, _BackBufferFormat);

		ReleaseCom(output);

		++i;
	}
}

void D3DApp::LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format)
{
	UINT count = 0;
	UINT flags = 0;

	// Call with nullptr to get list count.
	output->GetDisplayModeList(format, flags, &count, nullptr);

	std::vector<DXGI_MODE_DESC> modeList(count);
	output->GetDisplayModeList(format, flags, &count, &modeList[0]);

	for (auto& x : modeList)
	{
		UINT n = x.RefreshRate.Numerator;
		UINT d = x.RefreshRate.Denominator;
		std::wstring text =
			L"Width = " + std::to_wstring(x.Width) + L" " +
			L"Height = " + std::to_wstring(x.Height) + L" " +
			L"Refresh = " + std::to_wstring(n) + L"/" + std::to_wstring(d) +
			L"\n";

		::OutputDebugString(text.c_str());
	}
}
#pragma endregion