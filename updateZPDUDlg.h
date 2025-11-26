
// updateZPDUDlg.h : 头文件
//

#pragma once
#include "afxwin.h"


// CupdateZPDUDlg 对话框
class CupdateZPDUDlg : public CDialogEx
{
// 构造
public:
	CupdateZPDUDlg(CWnd* pParent = NULL);	// 标准构造函数

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_UPDATEZPDU_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持


// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedUpdate();
	afx_msg void OnBnClickedChooseBtn();
	bool etLocalAdaptersInfo();
	bool etLocalAdaptersInfoEnd(); 
	void hideControl();
	void showControl();
	bool IsValidIPv4(const CString& ip);
	bool IsValidIPv6(const CString& ip);
	void fun();
	
	CComboBox m_ComboBox;
	CButton m_BootLoader;
	CButton m_Kernel;
	CButton m_App;
	afx_msg void OnBnClickedCheck1();
	afx_msg void OnBnClickedCheck2();
	afx_msg void OnBnClickedCheck3();
	int m_boot;
	int m_kernel;
	int m_app;
	int m_batch;
	afx_msg void OnCbnDropdownCombo1();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg LRESULT OnMyMessage(WPARAM w ,LPARAM l);
	afx_msg LRESULT OnMyProgressMessage(WPARAM w, LPARAM l);
	afx_msg LRESULT OnMyStartTimerMessage(WPARAM w, LPARAM l);
	afx_msg void OnCbnSelchangeCombo1();
	CComboBox m_method;
	afx_msg void OnCbnSelchangeCombo2();
	CComboBox m_endIPAddress;
	afx_msg void OnCbnSelchangeCombo3();
	afx_msg void OnCbnDropdownCombo3();
	afx_msg LRESULT OnUpdateEdit(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnUpdateErrorEdit(WPARAM wParam, LPARAM lParam);
	CEdit m_editOK;
	CEdit m_editError;
};
