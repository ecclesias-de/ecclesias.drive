On Error Resume Next
Set sh = CreateObject("WScript.Shell")
Set locator = CreateObject("WbemScripting.SWbemLocator")
Set provider = locator.ConnectServer(".", "root\default").Get("StdRegProv")

Const HKEY_CURRENT_USER = &H80000001
pathCLSID = "Software\Classes\CLSID"

' Scan through all custom structural keys inside HKCU
provider.EnumKey HKEY_CURRENT_USER, pathCLSID, arrSubKeys

If Not IsNull(arrSubKeys) Then
    For Each subkey In arrSubKeys
        instancePath = pathCLSID & "\" & subkey & "\Instance"
        provider.GetStringValue HKEY_CURRENT_USER, instancePath, "CLSID", clsidValue

        ' Safely capture our target matching footprint identity
        If LCase(clsidValue) = lcase("{0E5AAE11-A475-4c5b-AB00-C66DE400274E}") Then
            provider.GetStringValue HKEY_CURRENT_USER, pathCLSID & "\" & subkey, "", defaultName

            ' Isolate targeted items explicitly named Tine Drive
            If defaultName = "Tine Drive" Then
                sh.Run "reg.exe delete ""HKCU\Software\Classes\CLSID\" & subkey & """ /f", 0, True
                sh.Run "reg.exe delete ""HKCU\Software\Classes\Wow6432Node\CLSID\" & subkey & """ /f", 0, True
                sh.Run "reg.exe delete ""HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\Desktop\NameSpace\" & subkey & """ /f", 0, True
            End If
        End If
    Next
End If
