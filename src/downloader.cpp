/*
 * Copyright 2018 Matthieu Gautier <mgautier@kymeria.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU  General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include "downloader.h"
#include "common/pathTools.h"

#include <algorithm>
#include <thread>
#include <chrono>

#include <iostream>

#include "aria2.h"
#include "xmlrpc.h"
#include "common/otherTools.h"
#include <pugixml.hpp>

namespace kiwix
{

void Download::updateStatus(bool follow)
{
  static std::vector<std::string> statusKey = {"status", "files", "totalLength",
                                               "completedLength", "followedBy",
                                               "downloadSpeed", "verifiedLength"};
  std::string strStatus;
  if(follow && !m_followedBy.empty()) {
    strStatus = mp_aria->tellStatus(m_followedBy, statusKey);
  } else {
    strStatus = mp_aria->tellStatus(m_did, statusKey);
  }
//  std::cout << strStatus << std::endl;
  MethodResponse response(strStatus);
  if (response.isFault()) {
    m_status = Download::UNKNOWN;
    return;
  }
  auto structNode = response.getParams().getParam(0).getValue().getStruct();
  auto _status = structNode.getMember("status").getValue().getAsS();
  auto status = _status == "active" ? Download::ACTIVE
              : _status == "waiting" ? Download::WAITING
              : _status == "paused" ? Download::PAUSED
              : _status == "error" ? Download::ERROR
              : _status == "complete" ? Download::COMPLETE
              : _status == "removed" ? Download::REMOVED
              : Download::UNKNOWN;
  if (status == COMPLETE) {
    try {
      auto followedByMember = structNode.getMember("followedBy");
      m_followedBy = followedByMember.getValue().getArray().getValue(0).getAsS();
      if (follow) {
        status = ACTIVE;
        updateStatus(true);
        return;
      }
    } catch (InvalidRPCNode& e) { }
  }
  m_status = status;
  m_totalLength = std::stoull(structNode.getMember("totalLength").getValue().getAsS());
  m_completedLength = std::stoull(structNode.getMember("completedLength").getValue().getAsS());
  m_downloadSpeed = std::stoull(structNode.getMember("downloadSpeed").getValue().getAsS());
  try {
    auto verifiedLengthValue = structNode.getMember("verifiedLength").getValue();
    m_verifiedLength = std::stoull(verifiedLengthValue.getAsS());
  } catch (InvalidRPCNode& e) { m_verifiedLength = 0; }
  auto filesMember = structNode.getMember("files");
  auto fileStruct = filesMember.getValue().getArray().getValue(0).getStruct();
  m_path = fileStruct.getMember("path").getValue().getAsS();
  auto urisArray = fileStruct.getMember("uris").getValue().getArray();
  int index = 0;
  m_uris.clear();
  while(true) {
    try {
      auto uriNode = urisArray.getValue(index++).getStruct().getMember("uri");
      m_uris.push_back(uriNode.getValue().getAsS());
    } catch(InvalidRPCNode& e) { break; }
  }
}

/* Constructor */
Downloader::Downloader() :
  mp_aria(new Aria2())
{
  for (auto gid : mp_aria->tellActive()) {
    m_knownDownloads[gid] = std::unique_ptr<Download>(new Download(mp_aria, gid));
    m_knownDownloads[gid]->updateStatus();
  }
}


/* Destructor */
Downloader::~Downloader()
{
}

void Downloader::close()
{
  mp_aria->close();
}

std::vector<std::string> Downloader::getDownloadIds() {
  std::vector<std::string> ret;
  for(auto& p:m_knownDownloads) {
    ret.push_back(p.first);
  }
  return ret;
}

DownloadedFile Downloader::download(const std::string& url) {
  DownloadedFile fileHandle;
  try {
    std::vector<std::string> status_key = {"status", "files"};
    auto download = startDownload(url);
    std::cerr << "gid is : " << download->getDid() << std::endl;
    pugi::xml_document ret;
    while(true) {
     download->updateStatus();
     std::cerr << "Status is " << download->getStatus() << std::endl;
     if (download->getStatus() == Download::COMPLETE) {
       fileHandle.success = true;
       fileHandle.path = download->getPath();
       std::cerr << "FilePath is " << fileHandle.path << std::endl;
     } else if (download->getStatus() == Download::ERROR) {
       fileHandle.success = false;
     } else {
       // [TODO] Be wise here.
       std::this_thread::sleep_for(std::chrono::microseconds(100000));
       continue;
     }
     break;
    }
  } catch (...) { std::cerr << "waautena " << std::endl; };
  return fileHandle;
}

Download* Downloader::startDownload(const std::string& uri)
{
  for (auto& p: m_knownDownloads) {
    auto& d = p.second;
    auto& uris = d->getUris();
    if (std::find(uris.begin(), uris.end(), uri) != uris.end())
      return d.get();
  }
  std::vector<std::string> uris = {uri};
  auto gid = mp_aria->addUri(uris);
  m_knownDownloads[gid] = std::unique_ptr<Download>(new Download(mp_aria, gid));
  return m_knownDownloads[gid].get();
}

Download* Downloader::getDownload(const std::string& did)
{
  try {
    return m_knownDownloads.at(did).get();
  } catch(exception& e) {
    for (auto gid : mp_aria->tellActive()) {
      if (gid == did) {
        m_knownDownloads[gid] = std::unique_ptr<Download>(new Download(mp_aria, gid));
        return m_knownDownloads[gid].get();
      }
    }
    throw e;
  }
}

}
