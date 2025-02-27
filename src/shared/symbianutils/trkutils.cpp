/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2009 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** Commercial Usage
**
** Licensees holding valid Qt Commercial licenses may use this file in
** accordance with the Qt Commercial License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Nokia.
**
** GNU Lesser General Public License Usage
**
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** If you are unsure which license is appropriate for your use, please
** contact the sales department at http://qt.nokia.com/contact.
**
**************************************************************************/

#include "trkutils.h"
#include <ctype.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QDate>
#include <QtCore/QDateTime>
#include <QtCore/QTime>

#define logMessage(s)  do { qDebug() << "TRKCLIENT: " << s; } while (0)

namespace trk {

TrkAppVersion::TrkAppVersion()
{
    reset();
}

void TrkAppVersion::reset()
{
    trkMajor = trkMinor= protocolMajor = protocolMinor = 0;
}

Session::Session()
{
    reset();
}

void Session::reset()
{
    cpuMajor = 0;
    cpuMinor = 0;
    bigEndian = 0;
    defaultTypeSize = 0;
    fpTypeSize = 0;
    extended1TypeSize = 0;
    extended2TypeSize = 0;
    pid = 0;
    tid = 0;
    codeseg = 0;
    dataseg = 0;

    currentThread = 0;
    libraries.clear();
    trkAppVersion.reset();
}

static QString formatCpu(int major, int minor)
{
    //: CPU description of an S60 device
    //: %1 major verison, %2 minor version
    //: %3 real name of major verison, %4 real name of minor version
    const QString str = QCoreApplication::translate("trk::Session", "CPU: v%1.%2%3%4");
    QString majorStr;
    QString minorStr;
    switch (major) {
    case 0x04:
        majorStr = " ARM";
        break;
    }
    switch (minor) {
    case 0x00:
        minorStr = " 920T";
        break;
    }
    return str.arg(major).arg(minor).arg(majorStr).arg(minorStr);
 }

QString formatTrkVersion(const TrkAppVersion &version)
{
    QString str = QCoreApplication::translate("trk::Session",
                                              "App TRK: v%1.%2 TRK protocol: v%3.%4");
    str = str.arg(version.trkMajor).arg(version.trkMinor);
    return str.arg(version.protocolMajor).arg(version.protocolMinor);
}

QString Session::deviceDescription(unsigned verbose) const
{
    if (!cpuMajor)
        return QString();

    //: s60description
    //: description of an S60 device
    //: %1 CPU description, %2 endianness
    //: %3 default type size (if any), %4 float size (if any)
    //: %5 TRK version
    QString msg = QCoreApplication::translate("trk::Session", "%1, %2%3%4, %5");
    QString endianness = bigEndian
                         ? QCoreApplication::translate("trk::Session", "big endian")
                         : QCoreApplication::translate("trk::Session", "little endian");
    msg = msg.arg(formatCpu(cpuMajor, cpuMinor)).arg(endianness);
    //: The separator in a list of strings
    QString defaultTypeSizeStr;
    QString fpTypeSizeStr;
    if (verbose && defaultTypeSize)
        //: will be inserted into s60description
        defaultTypeSizeStr = QCoreApplication::translate("trk::Session", ", type size: %1").arg(defaultTypeSize);
    if (verbose && fpTypeSize)
        //: will be inserted into s60description
        fpTypeSizeStr = QCoreApplication::translate("trk::Session", ", float size: %1").arg(fpTypeSize);
    msg = msg.arg(defaultTypeSizeStr).arg(fpTypeSizeStr);
    return msg.arg(formatTrkVersion(trkAppVersion));
}

// --------------

QByteArray decode7d(const QByteArray &ba)
{
    QByteArray res;
    res.reserve(ba.size());
    for (int i = 0; i < ba.size(); ++i) {
        byte c = byte(ba.at(i));
        if (c == 0x7d) {
            ++i;
            c = 0x20 ^ byte(ba.at(i));
        }
        res.append(c);
    }
    return res;
}

QByteArray encode7d(const QByteArray &ba)
{
    QByteArray res;
    res.reserve(ba.size() + 2);
    for (int i = 0; i < ba.size(); ++i) {
        byte c = byte(ba.at(i));
        if (c == 0x7e || c == 0x7d) {
            res.append(0x7d);
            res.append(0x20 ^ c);
        } else {
            res.append(c);
        }
    }
    return res;
}

// FIXME: Use the QByteArray based version below?
static inline QString stringFromByte(byte c)
{
    return QString::fromLatin1("%1").arg(c, 2, 16, QChar('0'));
}

SYMBIANUTILS_EXPORT QString stringFromArray(const QByteArray &ba, int maxLen)
{
    QString str;
    QString ascii;
    const int size = maxLen == -1 ? ba.size() : qMin(ba.size(), maxLen);
    for (int i = 0; i < size; ++i) {
        //if (i == 5 || i == ba.size() - 2)
        //    str += "  ";
        int c = byte(ba.at(i));
        str += QString("%1 ").arg(c, 2, 16, QChar('0'));
        if (i >= 8 && i < ba.size() - 2)
            ascii += QChar(c).isPrint() ? QChar(c) : QChar('.');
    }
    if (size != ba.size()) {
        str += "...";
        ascii += "...";
    }
    return str + "  " + ascii;
}

SYMBIANUTILS_EXPORT QByteArray hexNumber(uint n, int digits)
{
    QByteArray ba = QByteArray::number(n, 16);
    if (digits == 0 || ba.size() == digits)
        return ba;
    return QByteArray(digits - ba.size(), '0') + ba;
}

SYMBIANUTILS_EXPORT QByteArray hexxNumber(uint n, int digits)
{
    return "0x" + hexNumber(n, digits);
}

TrkResult::TrkResult() :
    code(0),
    token(0),
    isDebugOutput(false)
{
}

void TrkResult::clear()
{
    code = token= 0;
    isDebugOutput = false;
    data.clear();
    cookie = QVariant();
}

QString TrkResult::toString() const
{
    QString res = stringFromByte(code);
    res += QLatin1String(" [");
    res += stringFromByte(token);
    res += QLatin1Char(']');
    res += QLatin1Char(' ');
    res += stringFromArray(data);
    return res;
}

QByteArray frameMessage(byte command, byte token, const QByteArray &data, bool serialFrame)
{
    byte s = command + token;
    for (int i = 0; i != data.size(); ++i)
        s += data.at(i);
    byte checksum = 255 - (s & 0xff);
    //int x = s + ~s;
    //logMessage("check: " << s << checksum << x;

    QByteArray response;
    response.reserve(data.size() + 3);
    response.append(char(command));
    response.append(char(token));
    response.append(data);
    response.append(char(checksum));

    QByteArray encodedData = encode7d(response);

    QByteArray ba;
    ba.reserve(encodedData.size() + 6);
    if (serialFrame) {
        ba.append(char(0x01));
        ba.append(char(0x90));
        const ushort encodedSize = encodedData.size() + 2; // 2 x 0x7e
        appendShort(&ba, encodedSize, BigEndian);
    }
    ba.append(char(0x7e));
    ba.append(encodedData);
    ba.append(char(0x7e));

    return ba;
}

/* returns 0 if array doesn't represent a result,
otherwise returns the length of the result data */
ushort isValidTrkResult(const QByteArray &buffer, bool serialFrame)
{
    if (serialFrame) {
        // Serial protocol with length info
        if (buffer.length() < 4)
            return 0;
        if (buffer.at(0) != 0x01 || byte(buffer.at(1)) != 0x90)
            return 0;
        const ushort len = extractShort(buffer.data() + 2);
        return (buffer.size() >= len + 4) ? len : ushort(0);
    }
    // Frameless protocol without length info
    const char delimiter = char(0x7e);
    const int firstDelimiterPos = buffer.indexOf(delimiter);
    // Regular message delimited by 0x7e..0x7e
    if (firstDelimiterPos == 0) {
        const int endPos = buffer.indexOf(delimiter, firstDelimiterPos + 1);
        return endPos != -1 ? endPos + 1 - firstDelimiterPos : 0;
    }
    // Some ASCII log message up to first delimiter or all
    return firstDelimiterPos != -1 ? firstDelimiterPos : buffer.size();
}

bool extractResult(QByteArray *buffer, bool serialFrame, TrkResult *result, QByteArray *rawData)
{
    result->clear();
    if(rawData)
        rawData->clear();
    const ushort len = isValidTrkResult(*buffer, serialFrame);
    if (!len)
        return false;
    // handle receiving application output, which is not a regular command
    const int delimiterPos = serialFrame ? 4 : 0;
    if (buffer->at(delimiterPos) != 0x7e) {
        result->isDebugOutput = true;
        result->data = buffer->mid(delimiterPos, len);
        result->data.replace("\r\n", "\n");
        *buffer->remove(0, delimiterPos + len);
        return true;
    }
    // FIXME: what happens if the length contains 0xfe?
    // Assume for now that it passes unencoded!
    const QByteArray data = decode7d(buffer->mid(delimiterPos + 1, len - 2));
    if(rawData)
        *rawData = data;
    *buffer->remove(0, delimiterPos + len);

    byte sum = 0;
    for (int i = 0; i < data.size(); ++i) // 3 = 2 * 0xfe + sum
        sum += byte(data.at(i));
    if (sum != 0xff)
        logMessage("*** CHECKSUM ERROR: " << byte(sum));

    result->code = data.at(0);
    result->token = data.at(1);
    result->data = data.mid(2, data.size() - 3);
    //logMessage("   REST BUF: " << stringFromArray(*buffer));
    //logMessage("   CURR DATA: " << stringFromArray(data));
    //QByteArray prefix = "READ BUF:                                       ";
    //logMessage((prefix + "HEADER: " + stringFromArray(header).toLatin1()).data());
    return true;
}

SYMBIANUTILS_EXPORT ushort extractShort(const char *data)
{
    return byte(data[0]) * 256 + byte(data[1]);
}

SYMBIANUTILS_EXPORT uint extractInt(const char *data)
{
    uint res = byte(data[0]);
    res *= 256; res += byte(data[1]);
    res *= 256; res += byte(data[2]);
    res *= 256; res += byte(data[3]);
    return res;
}

SYMBIANUTILS_EXPORT QString quoteUnprintableLatin1(const QByteArray &ba)
{
    QString res;
    char buf[10];
    for (int i = 0, n = ba.size(); i != n; ++i) {
        const byte c = ba.at(i);
        if (isprint(c)) {
            res += c;
        } else {
            qsnprintf(buf, sizeof(buf) - 1, "\\%x", int(c));
            res += buf;
        }
    }
    return res;
}

SYMBIANUTILS_EXPORT void appendShort(QByteArray *ba, ushort s, Endianness endian)
{
    if (endian == BigEndian) {
        ba->append(s / 256);
        ba->append(s % 256);
    } else {
        ba->append(s % 256);
        ba->append(s / 256);
    }
}

SYMBIANUTILS_EXPORT void appendInt(QByteArray *ba, uint i, Endianness endian)
{
    const uchar b3 = i % 256; i /= 256;
    const uchar b2 = i % 256; i /= 256;
    const uchar b1 = i % 256; i /= 256;
    const uchar b0 = i;
    ba->reserve(ba->size() + 4);
    if (endian == BigEndian) {
        ba->append(b0);
        ba->append(b1);
        ba->append(b2);
        ba->append(b3);
    } else {
        ba->append(b3);
        ba->append(b2);
        ba->append(b1);
        ba->append(b0);
    }
}

void appendString(QByteArray *ba, const QByteArray &str, Endianness endian, bool appendNullTerminator)
{
    const int fullSize = str.size() + (appendNullTerminator ? 1 : 0);
    appendShort(ba, fullSize, endian); // count the terminating \0
    ba->append(str);
    if (appendNullTerminator)
        ba->append('\0');
}

void appendDateTime(QByteArray *ba, QDateTime dateTime, Endianness endian)
{
    // convert the QDateTime to UTC and append its representation to QByteArray
    // format is the same as in FAT file system
    dateTime = dateTime.toUTC();
    const QTime utcTime = dateTime.time();
    const QDate utcDate = dateTime.date();
    uint fatDateTime = (utcTime.hour() << 11 | utcTime.minute() << 5 | utcTime.second()/2) << 16;
    fatDateTime |= (utcDate.year()-1980) << 9 | utcDate.month() << 5 | utcDate.day();
    appendInt(ba, fatDateTime, endian);
}

QByteArray errorMessage(byte code)
{
    switch (code) {
        case 0x00: return "No error";
        case 0x01: return "Generic error in CWDS message";
        case 0x02: return "Unexpected packet size in send msg";
        case 0x03: return "Internal error occurred in CWDS";
        case 0x04: return "Escape followed by frame flag";
        case 0x05: return "Bad FCS in packet";
        case 0x06: return "Packet too long";
        case 0x07: return "Sequence ID not expected (gap in sequence)";

        case 0x10: return "Command not supported";
        case 0x11: return "Command param out of range";
        case 0x12: return "An option was not supported";
        case 0x13: return "Read/write to invalid memory";
        case 0x14: return "Read/write invalid registers";
        case 0x15: return "Exception occurred in CWDS";
        case 0x16: return "Targeted system or thread is running";
        case 0x17: return "Breakpoint resources (HW or SW) exhausted";
        case 0x18: return "Requested breakpoint conflicts with existing one";

        case 0x20: return "General OS-related error";
        case 0x21: return "Request specified invalid process";
        case 0x22: return "Request specified invalid thread";
    }
    return "Unknown error";
}

uint swapEndian(uint in)
{
    return (in>>24) | ((in<<8) & 0x00FF0000) | ((in>>8) & 0x0000FF00) | (in<<24);
}

int TrkResult::errorCode() const
{
    // NAK means always error, else data sized 1 with a non-null element
    const bool isNAK = code == 0xff;
    if (data.size() != 1 && !isNAK)
        return 0;
    if (const int errorCode = data.at(0))
        return errorCode;
    return isNAK ? 0xff : 0;
}

QString TrkResult::errorString() const
{
    // NAK means always error, else data sized 1 with a non-null element
    if (code == 0xff)
        return "NAK";
    if (data.size() < 1)
        return "Unknown error packet";
    return errorMessage(data.at(0));
}

} // namespace trk

