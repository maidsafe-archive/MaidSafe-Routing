/*  Copyright 2012 MaidSafe.net limited

    This MaidSafe Software is licensed to you under (1) the MaidSafe.net Commercial License,
    version 1.0 or later, or (2) The General Public License (GPL), version 3, depending on which
    licence you accepted on initial access to the Software (the "Licences").

    By contributing code to the MaidSafe Software, or to this project generally, you agree to be
    bound by the terms of the MaidSafe Contributor Agreement, version 1.0, found in the root
    directory of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also
    available at: http://www.maidsafe.net/licenses

    Unless required by applicable law or agreed to in writing, the MaidSafe Software distributed
    under the GPL Licence is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS
    OF ANY KIND, either express or implied.

    See the Licences for the specific language governing permissions and limitations relating to
    use of the MaidSafe Software.                                                                 */

// No header guard

#ifdef MAIDSAFE_WIN32
#pragma warning(push)
#pragma warning(disable : 4125) /* decimal digit terminates octal escape sequence */
#pragma warning(disable : 4127) /* conditional expression is constant */
// All the following are copied from qglobal.h, QT_NO_WARNINGS section.
#pragma warning( \
    disable : 4097) /* typedef-name 'identifier1' used as synonym for class-name 'identifier2' */
#pragma warning(                                                                      \
    disable : 4231) /* nonstandard extension used : 'extern' before template explicit \
                       instantiation */
#pragma warning( \
    disable : 4244) /* 'conversion' conversion from 'type1' to 'type2', possible loss of data */
#pragma warning(                                                                                \
    disable : 4251) /* class 'A' needs to have dll interface for to be used by clients of class \
                       'B'. */
#pragma warning(disable : 4275) /* non - DLL-interface classkey 'identifier' used as base for \
                                   DLL-interface classkey 'identifier' */
#pragma warning(disable : 4355) /* 'this' : used in base member initializer list */
#pragma warning(disable : 4514) /* unreferenced inline/local function has been removed */
#pragma warning(disable : 4530) /* C++ exception handler used, but unwind semantics are not \
                                   enabled. Specify -GX */
#pragma warning( \
    disable : 4660) /* template-class specialization 'identifier' is already instantiated */
#pragma warning(disable : 4706) /* assignment within conditional expression */
#pragma warning(disable : 4710) /* function not inlined */
#pragma warning(disable : 4786) /* truncating debug info after 255 characters */
#pragma warning( \
    disable : 4800) /* 'type' : forcing value to bool 'true' or 'false' (performance warning) */
#endif

#ifdef MAIDSAFE_APPLE
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
#endif

#ifdef MAIDSAFE_LINUX
#pragma GCC diagnostic push
#ifndef __clang__
#pragma GCC diagnostic ignored "-pedantic"
#endif
#pragma GCC diagnostic ignored "-Wstrict-overflow"
#pragma GCC diagnostic ignored "-Wfloat-equal"
#endif

#include "QtCore/QDebug"
#include "QtCore/QFile"
#include "QtCore/QTimer"
#include "QtWebKitWidgets/QWebFrame"
#include "QtWebKitWidgets/QWebPage"
#include "QtWidgets/QApplication"
