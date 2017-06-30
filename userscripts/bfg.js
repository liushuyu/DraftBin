// ==UserScript==
// @name         BigFishGames Downloader
// @namespace    http://tampermonkey.net/
// @version      0.1
// @description  Download trial games without installing BFG client.
// @author       liushuyu
// @require      https://cdnjs.cloudflare.com/ajax/libs/jquery/3.2.1/jquery.min.js
// @match        http://www.bigfishgames.com/games/*
// @grant        GM_xmlhttpRequest
// @connect      bigfishgames.com
// ==/UserScript==

(function() {
    'use strict';
    var downBtns = findDownloadBtn();
    if (!downBtns.length) {alert('Couldn\'t find download link!');return;}
    var stubLink = downBtns[0].getAttribute('href');
    ModifyPage(downBtns, stubLink);
})();

function startProcedure(stubLink) {
    extractRedirect(stubLink);
}

function findDownloadBtn() {
    return $('.bfg-col-xs-12.btn.btn-blue');
}

function ModifyPage(downBtns, stubLink) {
    $('.learn-more')[0].innerText = 'You have BigFishGames Downloader script enabled, you can download the game without client.';
    for (var i = 0; i < downBtns.length; i++) {
        downBtns[i].setAttribute('href', '#');
        downBtns[i].innerHTML = 'Play Now <span class="sub-text">Direct download</span>';
        var newBtn = downBtns[i].cloneNode(true);
        downBtns[i].parentNode.replaceChild(newBtn, downBtns[i]);
        newBtn.addEventListener('click', function (event) {
            $('.compatibility-warning').remove();
            startProcedure(stubLink);
        }, false);
    }
}

function extractRedirect(stubLink) {
    GM_xmlhttpRequest({
        url: stubLink,
        method: "HEAD",
        onload: function(response) {
            findMeta(response.finalUrl);
        }
    });
}

function findMeta(prepURL) {
    var metaData = {WID: getParameterByName("gameWID", prepURL), lang: getParameterByName("langID", prepURL), site: getParameterByName("siteID", prepURL)};
    buildReq(metaData);
}

function buildReq(metaData) {
    var email = prompt("Enter the e-mail address you used to register BigFishGames account:", "");
    if (!(/^\w+([\.-]?\w+)*@\w+([\.-]?\w+)*(\.\w{2,3})+$/.test(email))) {alert('Invaild e-mail address detected! Aborting.'); return;}
    var reqbody = '<?xml version="1.0" encoding="utf-8"?><methodCall><methodName>gms.getGameInfo</methodName><params><param><value><struct><member><name>gameWID</name><value><string>' + metaData.WID + '</string></value></member><member><name>siteID</name><value><string>' + metaData.site + '</string></value></member><member><name>languageID</name><value><string>' + metaData.lang + '</string></value></member><member><name>email</name><value><string>' + email + '</string></value></member><member><name>extData</name><value><string></string></value></member><member><name>downloadID</name><value><string>0</string></value></member></struct></value></param></params></methodCall>';
    GM_xmlhttpRequest({
        method: "POST",
        url: "https://gmservice.bigfishgames.com/index.php",
        data: reqbody,
        headers: {
            "Content-Type": "text/xml; charset=utf-8",
            "User-Agent": "Big Fish Games Client",
            "Host": "gmservice.bigfishgames.com"
        },
        onload: function(response) {
            parseResp(response.responseText);
        }
    });
}

function parseResp(resp) {
    var parser = new DOMParser();
    var xmldoc = parser.parseFromString(resp,"text/xml");
    var downloadInfo = xmldoc.getElementsByTagName('struct')[0].getElementsByTagName('member')[0].getElementsByTagName('struct');
    var cdns = downloadInfo[0].childNodes[3].getElementsByTagName('string');
    var fileInfo = downloadInfo[0].childNodes[5].getElementsByTagName('struct');
    var downloadFileInfo = [];
    for (var i = 0; i < fileInfo.length; i++) {
        var dataSet = fileInfo[i].getElementsByTagName('string');
        console.log(dataSet);
        downloadFileInfo.push({name: dataSet[0].innerHTML, url: cdns[0].innerHTML + '/' + dataSet[(fileInfo.length > 1 ? 4 : 3)].innerHTML});
    }
    processDownload(downloadFileInfo);
}

function processDownload(downData) {
   if (downData.length > 0) {
       htmlContent = "<html><head><title>BigFishGame direct download</title></head><body><h2>Your game need to download the following " + downData.length + " files</h2><ol>";
       for (var i = 0; i < downData.length; i++) {
           htmlContent += "<li><a href='" + downData[i].url + "'>Click to download file " + (i + 1) + "</a>";
       }
       htmlContent += "</ol></body></html>";
       var opened = window.open("");
       opened.document.write(htmlContent);
       return;
   }
    window.location = downData[0].url;
}

function getParameterByName(name, url) {
    if (!url) url = window.location.href;
    name = name.replace(/[\[\]]/g, "\\$&");
    var regex = new RegExp("[?&]" + name + "(=([^&#]*)|&|#|$)"),
        results = regex.exec(url);
    if (!results) return null;
    if (!results[2]) return '';
    return decodeURIComponent(results[2].replace(/\+/g, " "));
}

