VERSION 5.00
Object = "{248DD890-BB45-11CF-9ABC-0080C7E7B78D}#1.0#0"; "MSWINSCK.OCX"
Begin VB.UserControl OWNet 
   BackColor       =   &H0000FFFF&
   BackStyle       =   0  'Transparent
   CanGetFocus     =   0   'False
   ClientHeight    =   195
   ClientLeft      =   0
   ClientTop       =   0
   ClientWidth     =   1500
   ClipControls    =   0   'False
   FillStyle       =   0  'Solid
   ForwardFocus    =   -1  'True
   HasDC           =   0   'False
   InvisibleAtRuntime=   -1  'True
   ScaleHeight     =   195
   ScaleWidth      =   1500
   Begin MSWinsockLib.Winsock WS 
      Left            =   840
      Top             =   1575
      _ExtentX        =   741
      _ExtentY        =   741
      _Version        =   393216
   End
   Begin VB.Label Label1 
      AutoSize        =   -1  'True
      BackStyle       =   0  'Transparent
      Caption         =   "OWNET Control"
      BeginProperty Font 
         Name            =   "MS Sans Serif"
         Size            =   8.25
         Charset         =   0
         Weight          =   700
         Underline       =   0   'False
         Italic          =   0   'False
         Strikethrough   =   0   'False
      EndProperty
      ForeColor       =   &H00000000&
      Height          =   195
      Left            =   0
      TabIndex        =   0
      Top             =   0
      Width           =   1365
   End
End
Attribute VB_Name = "OWNet"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = True
Attribute VB_PredeclaredId = False
Attribute VB_Exposed = False
'       WE USE MICROSOFT WINSOCK CONTROL (VISUAL BASIC 5 OR 6)
'       IF YOU WANT TO USE WINSOCK API SEE WS_* FUNCTION ON END OF THIS FILE
'
' WE DON'T USE PROPERTIES! JUST PUBLIC VARIABLES AND SETHOST,SETPORT FUNCTIONS!!!
' TESTED WITH VB 5 AND VB 6, .NET MAY NOT WORK BECAUSE WE USE On Error Goto...
'
Option Explicit
' private variables
Private EXITING As Boolean
Private LAST_ERROR As OWNET_ERRORS
Private WS_BUFFER As String
' public variables
Public RemoteHost As String
Public RemotePort As Integer
'private constants
Private Const OWNET_DEFAULT_HOST = "127.0.0.1"
Private Const OWNET_DEFAULT_PORT = 1234
'public enum
Public Enum OWNET_READ_TYPES
        OWNET_MSG_ERROR = 0
        OWNET_MSG_NOP = 1
        OWNET_MSG_READ = 2
        OWNET_MSG_WRITE = 3
        OWNET_MSG_DIR = 4
        OWNET_MSG_SIZE = 5
        OWNET_MSG_PRESENCE = 6
        OWNET_MSG_READ_ANY = 99999
End Enum
Public Enum OWNET_ERRORS
        OWNET_ERR_NOERROR = 0
        OWNET_ERR_CANT_CONNECT = 1
        OWNET_ERR_READ_ERROR = 2
        OWNET_ERR_WRITE_ERROR = 3
        OWNET_ERR_NOREAD_VALUE = 4
End Enum
'OOP Like function
Public Sub SetHost(Host As String)
        Me.RemoteHost = Host
End Sub
Public Function GetHost() As String
        GetHost = Me.RemoteHost
End Function
Public Sub SetPort(Port As Integer)
        Me.RemotePort = Port
End Sub
Public Function GetPort() As Integer
        GetPort = Me.RemotePort
End Function
'public functions
Public Function Read(Path As String) As String                  ' READ A VALUE
        Read = OW_Get(Path, OWNET_READ_TYPES.OWNET_MSG_READ)
End Function
Public Function ReadAny(Path As String) As String               ' READ A VALUE OR A DIRECTORY
        ReadAny = OW_Get(Path, OWNET_READ_TYPES.OWNET_MSG_READ_ANY)
End Function
Public Function Dir(Path As String) As String                   ' READ A DIRECTORY
        ' Directory is return with "," separator, use SPLIT function to get array of directory informations
        Dir = OW_Get(Path, OWNET_READ_TYPES.OWNET_MSG_DIR)
End Function
Public Function Presence(Path As String) As Boolean             ' CHECK PRESENCE
        If (OW_Get(Path, OWNET_READ_TYPES.OWNET_MSG_PRESENCE) = "1") Then
                Presence = True
        Else
                Presence = False
        End If
End Function
Public Function SetValue(Path As String, Value As String) As Boolean    ' SET A VALUE TO A PATH
        SetValue = False
        Dim TMP_INPUT As String
        On Error Resume Next
        LAST_ERROR = OWNET_ERR_NOERROR          ' reset last error
        WS_CloseConnection                      ' close WS if opened
        DoEvents
        Err.Clear
        On Error GoTo CONNECT_ERROR             ' goto connect error
        If (Not WS_MakeConnection(Me.RemoteHost, Me.RemotePort)) Then
                WS_CloseConnection
                LAST_ERROR = OWNET_ERR_CANT_CONNECT
                SetValue = False
                Exit Function
        End If
        DoEvents
        Do While (WS_state() <> sckConnected)
                If (WS_state() = sckBadState Or WS_state() = sckClosed Or WS_state() = sckClosing Or WS_state() = sckConnectAborted Or WS_state() = sckError Or EXITING) Then
                        WS_CloseConnection
                        LAST_ERROR = OWNET_ERR_CANT_CONNECT
                        SetValue = False
                        Exit Function
                End If
                DoEvents
        Loop
        ' we are connected :)
        
        On Error Resume Next
        
        If (Not WS_Send(packdata(OWNET_READ_TYPES.OWNET_MSG_WRITE, Len(Path) + 1 + Len(Value) + 1, Len(Value) + 1))) Then
                LAST_ERROR = OWNET_ERR_WRITE_ERROR
                SetValue = False
                Exit Function
        End If
        If (Not WS_Send(Path & Chr(0) & Value & Chr(0))) Then
                LAST_ERROR = OWNET_ERR_WRITE_ERROR
                SetValue = False
                Exit Function
        End If
        TMP_INPUT = WS_Read(24)
        WS_CloseConnection
        If (Len(TMP_INPUT) <> 24) Then
                LAST_ERROR = OWNET_ERR_WRITE_ERROR
                SetValue = False
                Exit Function
        End If
        If (unpack_ntohl(TMP_INPUT, 2) <> 0) Then
                SetValue = False
        Else
                SetValue = True
        End If
        LAST_ERROR = OWNET_ERR_NOERROR
        WS_CloseConnection
        Exit Function
CONNECT_ERROR:
        Debug.Print "OWNET SETVALUE: ConnectError, WS State: " & WS_state() & " , ERR: " & Err.Number & ": " & Err.Description
        LAST_ERROR = OWNET_ERR_CANT_CONNECT
        Exit Function
End Function
Public Function LastError() As OWNET_ERRORS     ' GET LAST ERROR
        LastError = LAST_ERROR
End Function

'main read function
Private Function OW_Get(Path As String, get_type As OWNET_READ_TYPES) As String         ' READ ANYTHING THAT'S WE NEED
        Dim TMP_OUTPUT As String, TMP_INPUT As String, TMP_SIZE As Long, TMP_HEADER As String
        On Error Resume Next                    ' if an error occur resume next :)
        If (get_type = OWNET_READ_TYPES.OWNET_MSG_READ_ANY) Then
                OW_Get = OW_Get(Path, OWNET_READ_TYPES.OWNET_MSG_READ)  'GET READ
                If (LAST_ERROR <> OWNET_ERR_NOERROR) Then
                        OW_Get = OW_Get(Path, OWNET_READ_TYPES.OWNET_MSG_DIR)   'GET DIR
                End If
                Exit Function
        End If
        OW_Get = ""                             ' reset return
        LAST_ERROR = OWNET_ERR_NOERROR          ' reset last error
        WS_CloseConnection                      ' close WS if opened
        DoEvents                                ' be sure that we are connected
        Err.Clear                               ' clean errors
        On Error GoTo CONNECT_ERROR             ' goto connect error
        If (Not WS_MakeConnection(Me.RemoteHost, Me.RemotePort)) Then  '       can't connect
                WS_CloseConnection
                LAST_ERROR = OWNET_ERR_CANT_CONNECT
                OW_Get = ""
                Exit Function
        End If
        DoEvents
        Do While (WS_state() <> sckConnected)                   '       wait connect
                If (WS_state() = sckBadState Or WS_state() = sckClosed Or WS_state() = sckClosing Or WS_state() = sckConnectAborted Or WS_state() = sckError Or EXITING) Then
                        WS_CloseConnection
                        LAST_ERROR = OWNET_ERR_CANT_CONNECT
                        OW_Get = ""
                        Exit Function
                End If
                DoEvents
        Loop
        ' we are connected :)
        On Error Resume Next
        'On Error GoTo READ_ERROR
        
        If (get_type = OWNET_READ_TYPES.OWNET_MSG_DIR) Then ' get right send function
                OW_Get = ""
                TMP_OUTPUT = packdata(OWNET_READ_TYPES.OWNET_MSG_DIR, Len(Path) + 1, 0)
        ElseIf (get_type = OWNET_READ_TYPES.OWNET_MSG_PRESENCE) Then
                TMP_OUTPUT = packdata(OWNET_READ_TYPES.OWNET_MSG_PRESENCE, Len(Path) + 1, 0)
        Else
                get_type = OWNET_READ_TYPES.OWNET_MSG_READ
                TMP_OUTPUT = packdata(OWNET_READ_TYPES.OWNET_MSG_READ, Len(Path) + 1, 8192)
        End If
        ' sending request data to owserver
        If (Not WS_Send(TMP_OUTPUT)) Then
                LAST_ERROR = OWNET_ERR_READ_ERROR
                WS_CloseConnection
                OW_Get = ""
                Exit Function
        End If
        If (Not WS_Send(Path & Chr(0))) Then
                LAST_ERROR = OWNET_ERR_READ_ERROR
                WS_CloseConnection
                OW_Get = ""
                Exit Function
        End If
        ' sent data to owserver
        ' receiving
        LAST_ERROR = OWNET_ERR_NOREAD_VALUE
        Do While (True)
                TMP_HEADER = WS_Read(24)        ' get header
                If (Len(TMP_HEADER) <> 24) Then
                        WS_CloseConnection
                        LAST_ERROR = OWNET_ERR_READ_ERROR
                        OW_Get = ""
                        Exit Function
                End If
                If (get_type = OWNET_READ_TYPES.OWNET_MSG_PRESENCE) Then        'check presence
                        WS_CloseConnection
                        LAST_ERROR = OWNET_ERR_NOERROR
                        If (unpack_ntohl(TMP_HEADER, 2) = 0) Then
                                OW_Get = "1"                                    'return "1" when are present
                        Else
                                OW_Get = "0"                                    'return "0" when aren't present
                        End If
                        Exit Function
                End If
                If (unpack_ntohl(TMP_HEADER, 1) > 0) Then
                        If (get_type = OWNET_MSG_DIR) Then
                                TMP_SIZE = unpack_ntohl(TMP_HEADER, 1)
                        Else
                                TMP_SIZE = unpack_ntohl(TMP_HEADER, 2)
                        End If
                        TMP_INPUT = WS_Read(TMP_SIZE)
                        If (Len(TMP_INPUT) <> TMP_SIZE) Then                    'winsock closed?
                                WS_CloseConnection
                                LAST_ERROR = OWNET_ERR_READ_ERROR
                                OW_Get = ""
                                Exit Function
                        End If
                        LAST_ERROR = OWNET_ERR_NOERROR
                        If (get_type = OWNET_MSG_DIR) Then                      'get the directory
                                OW_Get = OW_Get & Mid(TMP_INPUT, 1, unpack_ntohl(TMP_HEADER, 4)) & ","  'apend directory entry
                        Else
                                OW_Get = Mid(TMP_INPUT, 1, TMP_SIZE)            'return value
                                Exit Do
                        End If
                Else
                        Exit Do
                End If
        Loop
        WS_CloseConnection
        If (get_type = OWNET_MSG_DIR And Len(OW_Get) > 0) Then  ' check if we are reading directory
                OW_Get = Mid(OW_Get, 1, Len(OW_Get) - 1) 'remove last ","
        End If
        Exit Function
CONNECT_ERROR:
        Debug.Print "OWNET READ: ConnectError, WS State: " & WS_state() & " , ERR: " & Err.Number & ": " & Err.Description
        LAST_ERROR = OWNET_ERR_CANT_CONNECT
        Exit Function
READ_ERROR:
        Debug.Print "OWNET READ: ReadError, WS State: " & WS_state() & " , ERR: " & Err.Number & ": " & Err.Description
        LAST_ERROR = OWNET_ERR_READ_ERROR
        Exit Function
End Function
Private Function packhtonl(Number As Long) As String    'built in pack function might work (got from ownet.php)
        Dim b1 As Single, b2 As Single, b3 As Single, b4 As Single
        b1 = ((Number And &HFF000000) / &H100 / &H100 / &H100) Mod &H100        'first  8 bits
        b2 = ((Number And &HFF0000) / &H100 / &H100) Mod &H100                  'second 8 bits
        b3 = ((Number And &HFF00) / &H100) Mod &H100                            'third  8 bits
        b4 = Number Mod &H100                                                   'fourth 8 bits
        packhtonl = Chr(b1) & Chr(b2) & Chr(b3) & Chr(b4)
End Function
Private Function unpack_ntohl(str As String, position As Single, Optional bit As Single = -1) As Long   'built in unpack function might work (got from ownet.php)
        Dim tmp_str As String, b1 As Single, b2 As Single, b3 As Single, b4 As Single
        On Error Resume Next
        unpack_ntohl = 0
        tmp_str = Mid(str, position * 4 + 1, 4)
        b1 = Asc(Mid(tmp_str, 1, 1))
        b2 = Asc(Mid(tmp_str, 2, 1))
        b3 = Asc(Mid(tmp_str, 3, 1))
        b4 = Asc(Mid(tmp_str, 4, 1))
        If (bit = 1) Then
                unpack_ntohl = b4
        ElseIf (bit = 2) Then
                unpack_ntohl = b3
        ElseIf (bit = 3) Then
                unpack_ntohl = b2
        ElseIf (bit = 4) Then
                unpack_ntohl = b1
        Else
                unpack_ntohl = b1 * &H1000000   ' can overflow!
                unpack_ntohl = unpack_ntohl + b4 + b3 * &H100 + b2 * &H10000    'avoid overflow, if occur we don't get first 8 bits
        End If
        
End Function
Private Function packdata(Function_Number As OWNET_READ_TYPES, Payload_Len As Long, Data_Length As Long) As String      ' (got from ownet.php)
        packdata = packhtonl(0) & _
                   packhtonl(Payload_Len) & _
                   packhtonl(Function_Number) & _
                   packhtonl(258) & _
                   packhtonl(Data_Length) & _
                   packhtonl(0)
End Function
' user control start and end functions
Private Sub UserControl_Initialize()
        EXITING = False
        Me.RemoteHost = OWNET_DEFAULT_HOST
        Me.RemotePort = OWNET_DEFAULT_PORT
End Sub
Private Sub UserControl_Terminate()
        On Error Resume Next
        EXITING = True
        WS_CloseConnection
End Sub

'       Winsock functions these functions as WINSOCK Control dependents,
'       if you want to change an use winsock API you just need to set NEW functions to inteface with API:
'               ws_makeconnection
'               ws_closeconnection
'               ws_send
'               ws_read
'               ws_status
'       sck* constants:
'               sckConnected
'               sckBadState
'               sckClosed
'               sckClosing
'               sckConnectAborted
'               sckError
'
Private Function WS_state() As Long
        WS_state = WS.State
End Function
Private Function WS_MakeConnection(Host As String, Port As Single) As Boolean
        WS_MakeConnection = False
        On Error Resume Next
        WS.Close
        On Error GoTo err_connect
        WS.RemoteHost = Host
        WS.RemotePort = Port
        WS.LocalPort = 0
        WS.Connect
        WS_MakeConnection = True
        Exit Function
err_connect:
        WS_MakeConnection = False
End Function
Private Function WS_Send(Output As String) As Boolean
        On Error GoTo ErrSending
        WS.SendData Output
        WS_Send = True
        Exit Function
ErrSending:
        WS_Send = False
End Function
Private Function WS_Read(ReadSize As Long) As String
        On Error Resume Next
        Do While (Len(WS_Read) < ReadSize)
                If (WS.State = sckConnected) Then
                        DoEvents
                        If (WS.BytesReceived > 0) Then
                                WS_DataArrival WS.BytesReceived
                        End If
                End If
                WS_Read = WS_Read & Mid(WS_BUFFER, 1, ReadSize)
                WS_BUFFER = Mid(WS_BUFFER, ReadSize + 1)
                If (WS.State <> sckConnected Or EXITING) Then
                        Exit Function
                End If
        Loop
End Function
Private Sub WS_CloseConnection()
        On Error Resume Next
        WS.Close
        WS_BUFFER = ""
End Sub
Private Sub WS_DataArrival(ByVal bytesTotal As Long)
        Dim tmp_buffer As String
        On Error Resume Next
        If (bytesTotal > 0) Then
                tmp_buffer = ""
                WS.GetData tmp_buffer
                If (Len(tmp_buffer) > 0) Then
                        WS_BUFFER = WS_BUFFER & tmp_buffer
                End If
        End If
End Sub
