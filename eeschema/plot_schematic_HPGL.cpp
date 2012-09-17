/** @file plot_schematic_HPGL.cpp
 */

/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 1992-2010 Jean-Pierre Charras <jean-pierre.charras@gipsa-lab.inpg.fr
 * Copyright (C) 1992-2010 KiCad Developers, see change_log.txt for contributors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you may find one here:
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 * or you may search the http://www.gnu.org website for the version 2 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include <fctsys.h>
#include <plot_common.h>
#include <worksheet.h>
#include <class_sch_screen.h>
#include <wxEeschemaStruct.h>
#include <base_units.h>
#include <sch_sheet_path.h>

#include <dialog_plot_schematic.h>


enum HPGL_PAGEZ_T {
    PAGE_DEFAULT = 0,
    HPGL_PAGE_SIZE_A4,
    HPGL_PAGE_SIZE_A3,
    HPGL_PAGE_SIZE_A2,
    HPGL_PAGE_SIZE_A1,
    HPGL_PAGE_SIZE_A0,
    HPGL_PAGE_SIZE_A,
    HPGL_PAGE_SIZE_B,
    HPGL_PAGE_SIZE_C,
    HPGL_PAGE_SIZE_D,
    HPGL_PAGE_SIZE_E,
};


static const wxChar* plot_sheet_list( int aSize )
{
    const wxChar* ret;

    switch( aSize )
    {
    default:
    case PAGE_DEFAULT:
        ret = NULL;         break;

    case HPGL_PAGE_SIZE_A4:
        ret = wxT( "A4" );  break;

    case HPGL_PAGE_SIZE_A3:
        ret = wxT( "A3" );  break;

    case HPGL_PAGE_SIZE_A2:
        ret = wxT( "A2" );  break;

    case HPGL_PAGE_SIZE_A1:
        ret = wxT( "A1" );  break;

    case HPGL_PAGE_SIZE_A0:
        ret = wxT( "A0" );  break;

    case HPGL_PAGE_SIZE_A:
        ret = wxT( "A" );   break;

    case HPGL_PAGE_SIZE_B:
        ret = wxT( "B" );   break;

    case HPGL_PAGE_SIZE_C:
        ret = wxT( "C" );   break;

    case HPGL_PAGE_SIZE_D:
        ret = wxT( "D" );   break;

    case HPGL_PAGE_SIZE_E:
        ret = wxT( "E" );   break;
    }

    return ret;
};


void DIALOG_PLOT_SCHEMATIC::SetHPGLPenWidth()
{
    g_HPGL_Pen_Descr.m_Pen_Diam = ReturnValueFromTextCtrl( *m_penHPGLWidthCtrl );

    if( g_HPGL_Pen_Descr.m_Pen_Diam > Millimeter2iu( 2 ) )
        g_HPGL_Pen_Descr.m_Pen_Diam = Millimeter2iu( 2 );

    if( g_HPGL_Pen_Descr.m_Pen_Diam < Millimeter2iu( 0.01 ) )
        g_HPGL_Pen_Descr.m_Pen_Diam = Millimeter2iu( 0.01 );
}


void DIALOG_PLOT_SCHEMATIC::createHPGLFile( bool aPlotAll )
{
    wxString        plotFileName;
    SCH_SCREEN*     screen = m_parent->GetScreen();
    SCH_SHEET_PATH* sheetpath;
    SCH_SHEET_PATH  oldsheetpath = m_parent->GetCurrentSheet();

    /* When printing all pages, the printed page is not the current page.
     *  In complex hierarchies, we must setup references and other parameters
     *  in the printed SCH_SCREEN
     *  because in complex hierarchies a SCH_SCREEN (a schematic drawings)
     *  is shared between many sheets
     */
    SCH_SHEET_LIST  SheetList( NULL );

    sheetpath = SheetList.GetFirst();
    SCH_SHEET_PATH  list;

    SetHPGLPenWidth();

    while( true )
    {
        if( aPlotAll )
        {
            if( sheetpath == NULL )
                break;

            list.Clear();

            if( list.BuildSheetPathInfoFromSheetPathValue( sheetpath->Path() ) )
            {
                m_parent->SetCurrentSheet( list );
                m_parent->GetCurrentSheet().UpdateAllScreenReferences();
                m_parent->SetSheetNumberAndCount();

                screen = m_parent->GetCurrentSheet().LastScreen();

                if( !screen ) // LastScreen() may return NULL
                    screen = m_parent->GetScreen();
            }
            else // Should not happen
                return;

            sheetpath = SheetList.GetNext();
        }

        const PAGE_INFO&    curPage = screen->GetPageSettings();

        PAGE_INFO           plotPage = curPage;

        // if plotting on a page size other than curPage
        if( m_HPGLPaperSizeOption->GetSelection() != PAGE_DEFAULT )
            plotPage.SetType( plot_sheet_list( m_HPGLPaperSizeOption->GetSelection() ) );

        // Calculation of conversion scales.
        double  plot_scale = (double) plotPage.GetWidthMils() / curPage.GetWidthMils();

        // Calculate offsets
        wxPoint plotOffset;

        if( GetPlotOriginCenter() )
        {
            plotOffset.x    = -plotPage.GetWidthIU() / 2;
            plotOffset.y    = -plotPage.GetHeightIU() / 2;
        }

        plotFileName = m_parent->GetUniqueFilenameForCurrentSheet() + wxT( "." )
                       + HPGL_PLOTTER::GetDefaultFileExtension();

        LOCALE_IO toggle;

        Plot_1_Page_HPGL( plotFileName, screen, plotPage, plotOffset, plot_scale );

        if( !aPlotAll )
            break;
    }

    m_parent->SetCurrentSheet( oldsheetpath );
    m_parent->GetCurrentSheet().UpdateAllScreenReferences();
    m_parent->SetSheetNumberAndCount();
}


void DIALOG_PLOT_SCHEMATIC::Plot_1_Page_HPGL( const wxString&   FileName,
                                              SCH_SCREEN*       screen,
                                              const PAGE_INFO&  pageInfo,
                                              wxPoint&          offset,
                                              double            plot_scale )
{
    wxString    msg;

    FILE*       output_file = wxFopen( FileName, wxT( "wt" ) );

    if( output_file == NULL )
    {
        msg = wxT( "\n** " );
        msg += _( "Unable to create " ) + FileName + wxT( " **\n" );
        m_MessagesBox->AppendText( msg );
        return;
    }

    LOCALE_IO toggle;

    msg.Printf( _( "Plot: %s " ), FileName.GetData() );
    m_MessagesBox->AppendText( msg );

    HPGL_PLOTTER* plotter = new HPGL_PLOTTER();

    plotter->SetPageSettings( pageInfo );
    plotter->SetViewport( offset, IU_PER_DECIMILS, plot_scale, false );

    // Init :
    plotter->SetCreator( wxT( "Eeschema-HPGL" ) );
    plotter->SetFilename( FileName );
    plotter->SetPenSpeed( g_HPGL_Pen_Descr.m_Pen_Speed );
    plotter->SetPenNumber( g_HPGL_Pen_Descr.m_Pen_Num );
    plotter->SetPenDiameter( g_HPGL_Pen_Descr.m_Pen_Diam );
    plotter->SetPenOverlap( g_HPGL_Pen_Descr.m_Pen_Diam / 2 );
    plotter->StartPlot( output_file );

    plotter->SetColor( BLACK );

    if( getPlotFrameRef() )
        PlotWorkSheet( plotter, m_parent->GetTitleBlock(),
                       m_parent->GetPageSettings(),
                       screen->m_ScreenNumber, screen->m_NumberOfScreens,
                       m_parent->GetScreenDesc(),
                       screen->GetFileName() );

    screen->Plot( plotter );

    plotter->EndPlot();
    delete plotter;

    m_MessagesBox->AppendText( wxT( "Ok\n" ) );
}