/******************************************************************************
 * Copyright (c) 2014-2015, Beijing HengShengDongYang Technology Ltd. All rights reserved.
 *
 *    Permission to use, copy, modify, and/or distribute this software for any
 *    purpose with or without fee is hereby granted, provided that the above
 *    copyright notice and this permission notice appear in all copies.
 *
 *    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *    MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *    ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *    WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *    ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 ******************************************************************************/
#if defined(QCC_OS_GROUP_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#endif
#include "Common/CommonUtils.h"
#include <qcc/Debug.h>
#include <qcc/StringUtil.h>

#if defined( _MSC_VER )
#include <direct.h>        // _getcwd
// #include <crtdbg.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(MINGW32) || defined(__MINGW32__)
#include <io.h>  // mkdir
#else
#include <sys/stat.h>    // mkdir
#endif

#define QCC_MODULE "SIPE2E"

using namespace ajn;
using namespace qcc;
using namespace std;

namespace sipe2e {

namespace gateway {

QStatus FillPropertyStore(services::AboutPropertyStoreImpl* propertyStore, const String& aboutKey, const String& aboutValue)
{
#define MANIPULATE_ARRAY_DICT(setMethod) \
    do { \
        size_t pos = 0; \
        do { \
            size_t searchPos = aboutValue.find_first_of('`', pos); \
            String manu = aboutValue.substr(pos, (String::npos==searchPos ? (aboutValue.length() - pos) : (searchPos - pos))); \
            pos = (String::npos == searchPos ? searchPos : searchPos + 1); \
            \
            /* Manipulate the manufacturer name string consisting of language and name in specific language */ \
            String lan, deviceName; \
            size_t posLan = manu.find_first_of(','); \
            if (String::npos != posLan) { \
                lan = manu.substr(0, posLan); \
                deviceName = manu.substr(posLan + 1, manu.length() - posLan - 1); \
            } else { \
                lan = "en"; \
                deviceName = manu; \
            } \
            status = propertyStore->setMethod(deviceName, lan); \
        } while (String::npos != pos); \
    } while (0);

    QStatus status = ER_OK;
    if (aboutKey == "DeviceId") {
        status = propertyStore->setDeviceId(aboutValue);
    } else if (aboutKey == "DeviceName") {
        /* The manufacturer name is like "en,device name`zh,设备名字" */
        MANIPULATE_ARRAY_DICT(setDeviceName);
    } else if (aboutKey == "AppId") {
        status = propertyStore->setAppId(aboutValue);
    } else if (aboutKey == "AppName") {
        /* The manufacturer name is like "en,app name`zh,app名字" */
        MANIPULATE_ARRAY_DICT(setAppName);
    } else if (aboutKey == "DefaultLanguage") {
        status = propertyStore->setDefaultLang(aboutValue);
    } else if (aboutKey == "SupportedLanguages") {
        /* The supported languages are marshaled like "en,zh,sp" */
        vector<String> supportedLanguages;
        size_t pos = 0;
        do {
            size_t searchPos = aboutValue.find_first_of(',', pos);
            String lan = aboutValue.substr(pos, (String::npos==searchPos ? (aboutValue.length() - pos) : (searchPos - pos)));
            supportedLanguages.push_back(lan);
            pos = (String::npos == searchPos ? searchPos : searchPos + 1);
        } while (String::npos != pos);
        status = propertyStore->setSupportedLangs(supportedLanguages);
    } else if (aboutKey == "Description") {
        /* The manufacturer name is like "en,description words`zh,介绍" */
        MANIPULATE_ARRAY_DICT(setDescription);
    } else if (aboutKey == "Manufacturer") {
        /* The manufacturer name is like "en,manufacturer name`zh,设备商名字" */
        MANIPULATE_ARRAY_DICT(setManufacturer);
    } else if (aboutKey == "DateOfManufacture") {
        status = propertyStore->setDateOfManufacture(aboutValue);
    } else if (aboutKey == "ModelNumber") {
        status = propertyStore->setModelNumber(aboutValue);
    } else if (aboutKey == "SoftwareVersion") {
        status = propertyStore->setSoftwareVersion(aboutValue);
    } else if (aboutKey == "AJSoftwareVersion") {
        status = propertyStore->setAjSoftwareVersion(aboutValue);
    } else if (aboutKey == "HardwareVersion") {
        status = propertyStore->setHardwareVersion(aboutValue);
    } else if (aboutKey == "SupportUrl") {
        status = propertyStore->setSupportUrl(aboutValue);
    }
    return status;
}

QStatus NormalizePath(qcc::String& strPath)
{
    char currPath[MAX_PATH];
    currPath[0] = '\0';
    char* tmpRet = NULL;
#ifdef _WIN32
    tmpRet = _getcwd(currPath, MAX_PATH);
#else // linux
    tmpRet = getcwd(currPath, MAX_PATH);
#endif
    if (NULL == tmpRet) {// indicating an error, like the max length exceeded
        return ER_FAIL;
    }
    if (strPath[0] == '.') {
        if (strPath[1] == '.') {
            // some relative path like '../test/test', just replace the '..' with upper directory
#ifdef _WIN32
            tmpRet = strrchr(currPath, '\\');
#else // Linux
            tmpRet = strrchr(currPath, '/');
#endif
            if (NULL == tmpRet) {
                return ER_FAIL;
            }
            *tmpRet = '\0';
            strPath.erase(0, 2);
            strPath.insert(0, (const char*)currPath);
        } else {
            // some relative path like './test/test', just replace the '.' with the current directory
            strPath.erase(0, 1);
            strPath.insert(0, (const char*)currPath);
        }
    } else {
        if (strPath[0] == '\\' || strPath[0] == '/') {
            strPath.insert(0, (const char*)currPath);
        } else {
#ifdef _WIN32
            strcat(currPath, "\\");
#else
            strcat(currPath, "/");
#endif
            strPath.insert(0, (const char*)currPath);
        }
    }
    // Now we iterate the path, if find any '..' the delete the upper directory
    size_t dotPos = strPath.find_first_of("..");
    while (dotPos != qcc::String::npos) {
        if (dotPos <= 2) {
            // no upper directory available
            break;
        }
#ifdef _WIN32
        size_t slashPos = strPath.find_last_of('\\', dotPos - 2);
#else
        size_t slashPos = strPath.find_last_of('/', dotPos - 2);
#endif
        strPath.erase(slashPos, dotPos - slashPos + 2);
        dotPos = strPath.find_first_of("..", slashPos);
    }
    dotPos = strPath.find_first_of('.');
    while (dotPos != qcc::String::npos) {
        strPath.erase(dotPos, 2);
        dotPos = strPath.find_first_of('.', dotPos);
    }
    // Normalizing the path by converting '\' to '/'
    size_t slashPos = strPath.find_first_of('\\');
    while (slashPos != qcc::String::npos) {
        strPath[slashPos] = '/';
        slashPos = strPath.find_first_of('\\', slashPos);
    }
    return ER_OK;
}

size_t StringSplit(const String& inStr, char delim, std::vector<String>& arrayOut)
{
    size_t arraySize = 0;
    size_t space = 0, startPos = 0;
    size_t arrayStrSize = inStr.size();
    space = inStr.find_first_of(delim, startPos);
    while (space != String::npos) {
        arraySize++;
        const String& currArgStr = inStr.substr(startPos, space - startPos);
        arrayOut.push_back(currArgStr);
        startPos = space + 1;
        if (startPos >= arrayStrSize) {
            break;
        }
        space = inStr.find_first_of(delim, startPos);
    }

    return arraySize;
}


qcc::String ArgToXml(const ajn::MsgArg* args, size_t indent)
{
    qcc::String str;
    if (!args) {
        return str;
    }
#define CHK_STR(s)  (((s) == NULL) ? "" : (s))
    qcc::String in = qcc::String(indent, ' ');

    str = in;

    indent += 2;

    switch (args->typeId) {
    case ALLJOYN_ARRAY:
        str += "<array type_sig=\"" + qcc::String(CHK_STR(args->v_array.GetElemSig())) + "\">";
        for (uint32_t i = 0; i < args->v_array.GetNumElements(); i++) {
            str += "\n" + ArgToXml(&args->v_array.GetElements()[i], indent)/*args->v_array.elements[i].ToString(indent)*/;
        }
        str += "\n" + in + "</array>";
        break;

    case ALLJOYN_BOOLEAN:
        str += args->v_bool ? "<boolean>1</boolean>" : "<boolean>0</boolean>";
        break;

    case ALLJOYN_DOUBLE:
        // To be bit-exact stringify double as a 64 bit hex value
        str += "<double>" "0x" + U64ToString(args->v_uint64, 16) + "</double>";
        break;

    case ALLJOYN_DICT_ENTRY:
        str += "<dict_entry>\n" +
            ArgToXml(args->v_dictEntry.key, indent) + "\n" + 
            //             args->v_dictEntry.key->ToString(indent) + "\n" +
            //             args->v_dictEntry.val->ToString(indent) + "\n" +
            ArgToXml(args->v_dictEntry.val, indent) + "\n" + 
            in + "</dict_entry>";
        break;

    case ALLJOYN_SIGNATURE:
        str += "<signature>" + qcc::String(CHK_STR(args->v_signature.sig)) + "</signature>";
        break;

    case ALLJOYN_INT32:
        str += "<int32>" + I32ToString(args->v_int32) + "</int32>";
        break;

    case ALLJOYN_INT16:
        str += "<int16>" + I32ToString(args->v_int16) + "</int16>";
        break;

    case ALLJOYN_OBJECT_PATH:
        str += "<object_path>" + qcc::String(CHK_STR(args->v_objPath.str)) + "</object_path>";
        break;

    case ALLJOYN_UINT16:
        str += "<uint16>" + U32ToString(args->v_uint16) + "</uint16>";
        break;

    case ALLJOYN_STRUCT:
        str += "<struct>\n";
        for (uint32_t i = 0; i < args->v_struct.numMembers; i++) {
            str += ArgToXml(&args->v_struct.members[i], indent)/*args->v_struct.members[i].ToString(indent)*/ + "\n";
        }
        str += in + "</struct>";
        break;

    case ALLJOYN_STRING:
        str += "<string>" + qcc::String(CHK_STR(args->v_string.str)) + "</string>";
        break;

    case ALLJOYN_UINT64:
        str += "<uint64>" + U64ToString(args->v_uint64) + "</uint64>";
        break;

    case ALLJOYN_UINT32:
        str += "<uint32>" + U32ToString(args->v_uint32) + "</uint32>";
        break;

    case ALLJOYN_VARIANT:
        str += "<variant signature=\"" + args->v_variant.val->Signature() + "\">\n";
        str += ArgToXml(args->v_variant.val, indent)/*args->v_variant.val->ToString(indent)*/;
        str += "\n" + in + "</variant>";
        break;

    case ALLJOYN_INT64:
        str += "<int64>" + I64ToString(args->v_int64) + "</int64>";
        break;

    case ALLJOYN_BYTE:
        str += "<byte>" + U32ToString(args->v_byte) + "</byte>";
        break;

    case ALLJOYN_HANDLE:
        str += "<handle>" + qcc::BytesToHexString((const uint8_t*)&args->v_handle.fd, sizeof(args->v_handle.fd)) + "</handle>";
        break;

    case ALLJOYN_BOOLEAN_ARRAY:
        str += "<array type=\"boolean\">";
        if (args->v_scalarArray.numElements) {
            str += "\n" + qcc::String(indent, ' ');
            for (uint32_t i = 0; i < args->v_scalarArray.numElements; i++) {
                str += args->v_scalarArray.v_bool[i] ? "1 " : "0 ";
            }
        }
        str += "\n" + in + "</array>";
        break;

    case ALLJOYN_DOUBLE_ARRAY:
        str += "<array type=\"double\">";
        if (args->v_scalarArray.numElements) {
            str += "\n" + qcc::String(indent, ' ');
            for (uint32_t i = 0; i < args->v_scalarArray.numElements; i++) {
                str += U64ToString((uint64_t)args->v_scalarArray.v_double[i]) + " ";
            }
        }
        str += "\n" + in + "</array>";
        break;

    case ALLJOYN_INT32_ARRAY:
        str += "<array type=\"int32\">";
        if (args->v_scalarArray.numElements) {
            str += "\n" + qcc::String(indent, ' ');
            for (uint32_t i = 0; i < args->v_scalarArray.numElements; i++) {
                str += I32ToString(args->v_scalarArray.v_int32[i]) + " ";
            }
        }
        str += "\n" + in + "</array>";
        break;

    case ALLJOYN_INT16_ARRAY:
        str += "<array type=\"int16\">";
        if (args->v_scalarArray.numElements) {
            str += "\n" + qcc::String(indent, ' ');
            for (uint32_t i = 0; i < args->v_scalarArray.numElements; i++) {
                str += I32ToString(args->v_scalarArray.v_int16[i]) + " ";
            }
        }
        str += "\n" + in + "</array>";
        break;

    case ALLJOYN_UINT16_ARRAY:
        str += "<array type=\"uint16\">";
        if (args->v_scalarArray.numElements) {
            str += "\n" + qcc::String(indent, ' ');
            for (uint32_t i = 0; i < args->v_scalarArray.numElements; i++) {
                str += U32ToString(args->v_scalarArray.v_uint16[i]) + " ";
            }
        }
        str += "\n" + in + "</array>";
        break;

    case ALLJOYN_UINT64_ARRAY:
        str += "<array type=\"uint64\">";
        if (args->v_scalarArray.numElements) {
            str += "\n" + qcc::String(indent, ' ');
            for (uint32_t i = 0; i < args->v_scalarArray.numElements; i++) {
                str += U64ToString(args->v_scalarArray.v_uint64[i]) + " ";
            }
        }
        str += "\n" + in + "</array>";
        break;

    case ALLJOYN_UINT32_ARRAY:
        str += "<array type=\"uint32\">";
        if (args->v_scalarArray.numElements) {
            str += "\n" + qcc::String(indent, ' ');
            for (uint32_t i = 0; i < args->v_scalarArray.numElements; i++) {
                str += U32ToString(args->v_scalarArray.v_uint32[i]) + " ";
            }
        }
        str += "\n" + in + "</array>";
        break;

    case ALLJOYN_INT64_ARRAY:
        str += "<array type=\"int64\">";
        if (args->v_scalarArray.numElements) {
            str += "\n" + qcc::String(indent, ' ');
            for (uint32_t i = 0; i < args->v_scalarArray.numElements; i++) {
                str += I64ToString(args->v_scalarArray.v_int64[i]) + " ";
            }
        }
        str += "\n" + in + "</array>";
        break;

    case ALLJOYN_BYTE_ARRAY:
        str += "<array type=\"byte\">";
        if (args->v_scalarArray.numElements) {
            str += "\n" + qcc::String(indent, ' ');
            for (uint32_t i = 0; i < args->v_scalarArray.numElements; i++) {
                str += U32ToString(args->v_scalarArray.v_byte[i]) + " ";
            }
        }
        str += "\n" + in + "</array>";
        break;

    default:
        str += "<invalid/>";
        break;
    }
    str += "\n";
#undef CHK_STR
    return str;
}

QStatus XmlToArg(const XmlElement* argEle, MsgArg& arg)
{
    QStatus status = ER_OK;

    const String& argType = argEle->GetName();
    // here calculate a simple hash value to use switch
    int32_t hashVal = 0;
    for (size_t i = 0; i < argType.size(); i++) {
        hashVal += argType[i];
    }

    switch (hashVal) {
    case 543: // array
        {
            const String& arrayType = argEle->GetAttribute("type");
            int32_t hashValArray = 0;
            for (size_t j = 0; j < arrayType.size(); j++) {
                hashValArray += arrayType[j];
            }

#define ParseArrayArg(StringToFunc, arraySig, argType) \
    const String& argStr = argEle->GetContent(); \
    std::vector<String> arrayTmp; \
    size_t arraySize = StringSplit(argStr, ' ', arrayTmp); \
    if (arraySize > 0) { \
        argType* argArray = new argType[arraySize]; \
        for (size_t indx = 0; indx < arraySize; indx++) { \
            argArray[indx] = StringToFunc(arrayTmp[indx]); \
        } \
        status = arg.Set(arraySig, arraySize, argArray); \
    }

            switch (hashValArray) {
            case 736: // boolean array
                {
                    const String& booleanStr = argEle->GetContent();
                    size_t booleanArraySize = booleanStr.size();
                    if (booleanArraySize > 0) {
                        bool* booleanArray = new bool[booleanArraySize];
                        for (size_t boolIndx = 0; boolIndx < booleanArraySize; boolIndx++) {
                            booleanArray[boolIndx] = (booleanStr[boolIndx] == '1' ? true : false);
                        }
                        status = arg.Set("ab", booleanArraySize, booleanArray);
                    } else {}
                }
                break;
            case 635: // double array
                {
                    ParseArrayArg(StringToDouble, "ad", double);
                }
                break;
            case 432: // int32 array
                {
                    ParseArrayArg(StringToI32, "ai", int32_t);
                }
                break;
            case 434: // int16 array
                {
                    ParseArrayArg(StringToI32, "an", int16_t);
                }
                break;
            case 551: // uint16 array
                {
                    ParseArrayArg(StringToU32, "aq", uint16_t);
                }
                break;
            case 554: // uint64 array
                {
                    ParseArrayArg(StringToU64, "at", uint64_t);
                }
                break;
            case 549: // uint32 array
                {
                    ParseArrayArg(StringToU32, "au", uint32_t);
                }
                break;
            case 437: // int64 array
                {
                    ParseArrayArg(StringToI64, "ax", int64_t);
                }
                break;
            case 436: // byte array
                {
                    ParseArrayArg(StringToU32, "ay", uint8_t);
                }
                break;
            default: // array of variant
                {
                    // fill the array of variant by traversing its children
                    const std::vector<XmlElement*>& argsVec = argEle->GetChildren();
                    size_t argsNum = argsVec.size();
                    if (argsNum > 0) {
                        MsgArg* argsArray = new MsgArg[argsNum];
                        for (size_t argIndx = 0; argIndx < argsNum; argIndx++) {
                            const XmlElement* currArgEle = argsVec[argIndx];
                            status = XmlToArg(currArgEle, argsArray[argIndx]);
                            if (ER_OK != status) {
                                return status;
                            }
                        }
                        status = arg.Set("av", argsNum, argsArray);
                    } else {}
                }
                break;
            }
        }
        break;
    case 736: // boolean
        {
            const String& argStr = argEle->GetContent();
            status = arg.Set("b", (argStr[0] == '1' ? true : false));
        }
        break;
    case 635: // double
        {
            const String& argStr = argEle->GetContent();
            status = arg.Set("d", StringToDouble(argStr));
        }
        break;
    case 1077: // dict_entry
        {
            const std::vector<XmlElement*>& entryEleVec = argEle->GetChildren();
            if (entryEleVec.size() != 2) {// should be one key and one value
                break;
            }
            arg.v_dictEntry.key = new MsgArg();
            arg.v_dictEntry.val = new MsgArg();
            status = XmlToArg(entryEleVec[0], *arg.v_dictEntry.key);
            if (ER_OK != status) {
                return status;
            }
            status = XmlToArg(entryEleVec[1], *arg.v_dictEntry.val);
        }
        break;
    case 978: // signature
        {
            const String& argStr = argEle->GetContent();
            status = arg.Set("g", strdup(argStr.c_str()));
        }
        break;
    case 432: // int32
        {
            const String& argStr = argEle->GetContent();
            status = arg.Set("i", StringToI32(argStr));
        }
        break;
    case 434: // int16
        {
            const String& argStr = argEle->GetContent();
            status = arg.Set("n", StringToI32(argStr));
        }
        break;
    case 1155: // object_path
        {
            const String& argStr = argEle->GetContent();
            status = arg.Set("o", strdup(argStr.c_str()));
        }
        break;
    case 551: // uint16
        {
            const String& argStr = argEle->GetContent();
            status = arg.Set("q", StringToU32(argStr));
        }
        break;
    case 677: // struct
        {
            // fill the array of variant by traversing its children
            const std::vector<XmlElement*>& argsVec = argEle->GetChildren();
            size_t argsNum = argsVec.size();
            if (argsNum > 0) {
                MsgArg* argsArray = new MsgArg[argsNum];
                for (size_t argIndx = 0; argIndx < argsNum; argIndx++) {
                    const XmlElement* currArgEle = argsVec[argIndx];
                    status = XmlToArg(currArgEle, argsArray[argIndx]);
                    if (ER_OK != status) {
                        return status;
                    }
                }
                arg.typeId = ALLJOYN_STRUCT;
                arg.v_struct.numMembers = argsNum;
                arg.v_struct.members = argsArray;
            } else {}
        }
        break;
    case 663: // string
        {
            const String& argStr = argEle->GetContent();
            status = arg.Set("s", strdup(argStr.c_str()));
        }
        break;
    case 554: // uint64
        {
            const String& argStr = argEle->GetContent();
            status = arg.Set("t", StringToU64(argStr));
        }
        break;
    case 549: // uint32
        {
            const String& argStr = argEle->GetContent();
            status = arg.Set("u", StringToU32(argStr));
        }
        break;
    case 757: // variant
        {
            // fill the array of variant by traversing its children
            const std::vector<XmlElement*>& argsVec = argEle->GetChildren();
            arg.typeId = ALLJOYN_VARIANT;
            arg.v_variant.val = new MsgArg();
            status = XmlToArg(argsVec[0], *arg.v_variant.val);
        }
        break;
    case 437: // int64
        {
            const String& argStr = argEle->GetContent();
            status = arg.Set("x", StringToI64(argStr));
        }
        break;
    case 436: // byte
        {
            const String& argStr = argEle->GetContent();
            status = arg.Set("y", StringToU32(argStr));
        }
        break;
    case 620: // handle
        {
            const String& argStr = argEle->GetContent();
            SocketFd fd;
            HexStringToBytes(argStr, (uint8_t*)&fd, 32);
            status = arg.Set("h", fd);
        }
        break;
    default:
        break;
    }
    return status;
}

void IllegalStringToObjPathString(const qcc::String& inStr, qcc::String& outStr)
{
    outStr.clear();
    for (size_t i = 0; i < inStr.size(); i++) {
        if (inStr[i] != '/') {
            outStr += qcc::U32ToString(inStr[i], 16);
        } else {
            outStr += "/";
        }
    }
}

void ObjPathStringToIllegalString(const qcc::String& inStr, qcc::String& outStr)
{
    outStr.clear();
    char buf[3];
    buf[2] = '\0';
    size_t in = 0;
    for (size_t i = 0; i < inStr.size(); i++) {
        if (inStr[i] != '/') {
            buf[in] = inStr[i];
            if (in & 1) {
                char c = qcc::StringToU32(qcc::String(buf), 16);
                outStr.push_back(c);
                in = 0;
                continue;
            }
            in++;
        } else {
            outStr.push_back('/');
            in = 0;
        }
    }
}




} // namespace gateway

} // namespace sipe2e