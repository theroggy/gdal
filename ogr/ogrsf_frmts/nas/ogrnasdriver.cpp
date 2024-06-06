/******************************************************************************
 *
 * Project:  OGR
 * Purpose:  OGRNASDriver implementation
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "cpl_conv.h"
#include "cpl_multiproc.h"
#include "nasreaderp.h"
#include "ogr_nas.h"

/************************************************************************/
/*                     OGRNASDriverIdentify()                           */
/************************************************************************/

static int OGRNASDriverIdentify(GDALOpenInfo *poOpenInfo)

{
    if (poOpenInfo->fpL == nullptr)
        return FALSE;

    /* -------------------------------------------------------------------- */
    /*      Check for a UTF-8 BOM and skip if found                         */
    /*                                                                      */
    /*      TODO: BOM is variable-length parameter and depends on encoding. */
    /*            Add BOM detection for other encodings.                    */
    /* -------------------------------------------------------------------- */

    // Used to skip to actual beginning of XML data
    const char *pszPtr = reinterpret_cast<const char *>(poOpenInfo->pabyHeader);

    // Skip UTF-8 BOM
    if (poOpenInfo->nHeaderBytes > 3 &&
        memcmp(poOpenInfo->pabyHeader, "\xEF\xBB\xBF", 3) == 0)
    {
        pszPtr += 3;
    }

    // Skip spaces
    while (*pszPtr && std::isspace(static_cast<unsigned char>(*pszPtr)))
        ++pszPtr;

    /* -------------------------------------------------------------------- */
    /*      Here, we expect the opening chevrons of NAS tree root element   */
    /* -------------------------------------------------------------------- */
    if (pszPtr[0] != '<')
        return FALSE;

    if (poOpenInfo->IsSingleAllowedDriver("NAS"))
        return TRUE;

    // TryToIngest() invalidates above pszPtr
    pszPtr = nullptr;
    CPL_IGNORE_RET_VAL(pszPtr);
    if (!poOpenInfo->TryToIngest(8192))
        return FALSE;
    pszPtr = reinterpret_cast<const char *>(poOpenInfo->pabyHeader);

    if (strstr(pszPtr, "opengis.net/gml") == nullptr)
        return FALSE;

    char **papszIndicators = CSLTokenizeStringComplex(
        CPLGetConfigOption("NAS_INDICATOR",
                           "NAS-Operationen;AAA-Fachschema;aaa.xsd;aaa-suite"),
        ";", 0, 0);

    bool bFound = false;
    for (int i = 0; papszIndicators[i] && !bFound; i++)
    {
        bFound = strstr(pszPtr, papszIndicators[i]) != nullptr;
    }

    CSLDestroy(papszIndicators);

    // Require NAS_GFS_TEMPLATE to be defined
    if (bFound && !CPLGetConfigOption("NAS_GFS_TEMPLATE", nullptr))
    {
        CPLDebug("NAS",
                 "This file could be recognized by the NAS driver. "
                 "If this is desired, you need to define the NAS_GFS_TEMPLATE "
                 "configuration option.");
        return FALSE;
    }

    return bFound;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRNASDriverOpen(GDALOpenInfo *poOpenInfo)

{
    if (poOpenInfo->eAccess == GA_Update || !OGRNASDriverIdentify(poOpenInfo))
        return nullptr;

    VSIFCloseL(poOpenInfo->fpL);
    poOpenInfo->fpL = nullptr;

    OGRNASDataSource *poDS = new OGRNASDataSource();

    if (!poDS->Open(poOpenInfo->pszFilename))
    {
        delete poDS;
        return nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                           RegisterOGRNAS()                           */
/************************************************************************/

void RegisterOGRNAS()

{
    if (GDALGetDriverByName("NAS") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("NAS");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "NAS - ALKIS");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "xml");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/vector/nas.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS, "OGRSQL SQLITE");

    poDriver->pfnOpen = OGRNASDriverOpen;
    poDriver->pfnIdentify = OGRNASDriverIdentify;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
