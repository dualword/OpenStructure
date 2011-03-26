//------------------------------------------------------------------------------
// This file is part of the OpenStructure project <www.openstructure.org>
//
// Copyright (C) 2008-2011 by the OpenStructure authors
//
// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License as published by the Free
// Software Foundation; either version 3.0 of the License, or (at your option)
// any later version.
// This library is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
// details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this library; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
//------------------------------------------------------------------------------
/*
  Authors: Marco Biasini, Andreas Schenk
 */

#include <iostream>


#include "python_shell_widget.hh"

#include "gutter.hh"

#include "string_literal_positions.hh"

#include "python_shell_text_document_layout.hh"

#include "python_completer.hh"
#include "path_completer.hh"
#include "transition.hh"

#include <QApplication>
#include <QFontMetrics>
#include <QClipboard>
#include <QPainter>
#include <QScrollBar>
#include <QDirModel>
#include <QStringList>
#include <QDebug>

/*
 Authors: Marco Biasini, Andreas Schenk
 */

namespace ost { namespace gui {


PythonShellWidget::PythonShellWidget(QWidget* parent):
  QPlainTextEdit(parent),
  tab_width_(2),
  highlighter_(document()),
  completer_(new PythonCompleter(viewport())),
  path_completer_(new PathCompleter(viewport())),
  block_edit_mode_(EDITMODE_SINGLELINE),
  gutter_(new Gutter(this)),
  output_visible_(true),
  completion_start_(0),
  completion_end_(0),
  block_edit_start_(document()->begin()),
  output_blocks_(),
  machine_(new StateMachine(this)),
  readonly_machine_(new StateMachine(this)),
  readwrite_state_(new State),
  multiline_active_state_(new State)
{
  setLineWrapMode(QPlainTextEdit::WidgetWidth);
  this->setFrameShape(QFrame::NoFrame);
  document()->setDocumentLayout(new PythonShellTextDocumentLayout(document()));
  setViewportMargins(Gutter::GUTTER_WIDTH, 0, 0, 0);
  setUndoRedoEnabled(false);
  setFont(QFont("Courier",font().pointSize()+2));
  QFontMetrics metrics(font());
  setTabStopWidth(2*metrics.width(" "));
  setMaximumBlockCount(1000000);

  textCursor().block().setUserState(BLOCKTYPE_ACTIVE);
  completer_->setWidget(viewport());
  connect(&PythonInterpreter::Instance(), SIGNAL(Output(unsigned int, const QString &)),
          this,SLOT(AppendOutput(unsigned int, const QString &)));
  connect(&PythonInterpreter::Instance(), SIGNAL(Finished(unsigned int, bool)),
          this,SLOT(OutputFinished(unsigned int,bool)));
  connect(&PythonInterpreter::Instance(), SIGNAL(ErrorOutput(unsigned int, const QString &)),
          this,SLOT(AppendError(unsigned int, const QString &)));
  connect(this,   SIGNAL(updateRequest(QRect, int)) ,
          gutter_,SLOT(Update(QRect, int)));
  connect(completer_,SIGNAL(activated(const QString&)),this, 
          SLOT(InsertCompletion(const QString&)));
  connect(completer_,SIGNAL(recomplete(const QString&)),this, 
          SLOT(Recomplete(const QString&)));
  connect(this,SIGNAL(RequestCompletion(const QRect&,bool)),completer_, 
          SLOT(complete(const QRect&,bool)));
  connect(this,SIGNAL(SetCompletionPrefix(const QString&)),completer_, 
          SLOT(SetCompletionPrefix(const QString&)));
  path_completer_->setWidget(viewport());
  connect(path_completer_,SIGNAL(activated(const QString&)),this, 
          SLOT(InsertPathCompletion(const QString&)));
  connect(path_completer_,SIGNAL(recomplete(const QString&)),this, 
          SLOT(Recomplete(const QString&)));
  connect(this,SIGNAL(RequestPathCompletion(const QRect&,bool)),path_completer_, 
          SLOT(complete(const QRect&,bool)));
  connect(this,SIGNAL(SetPathCompletionPrefix(const QString&)),path_completer_, 
          SLOT(setCompletionPrefix(const QString&)));
  set_block_edit_mode_(EDITMODE_SINGLELINE);
  setup_readonly_state_machine_();
  setup_state_machine_();
}

void PythonShellWidget::setup_readonly_state_machine_()
{
  State* readonly=new State;
  State* mixed=new State;
  readonly_machine_->addState(mixed);
  readonly_machine_->addState(readonly);
  readonly_machine_->addState(readwrite_state_);
  readonly->addTransition(new SignalTransition(this,
                                                SIGNAL(cursorPositionChanged()),
                                                readwrite_state_,
                                                new EditPositionGuard(this,EditPositionGuard::EQUAL |EditPositionGuard::BIGGER,
                                                                           EditPositionGuard::ANCHOREQUAL |EditPositionGuard::ANCHORBIGGER)));
  readonly->addTransition(new SignalTransition(this,
                                                SIGNAL(cursorPositionChanged()),
                                                mixed,
                                                new EditPositionGuard(this,EditPositionGuard::EQUAL |EditPositionGuard::BIGGER,
                                                                           EditPositionGuard::ANCHORSMALLER)));
  readonly->addTransition(new SignalTransition(this,
                                                SIGNAL(cursorPositionChanged()),
                                                mixed,
                                                new EditPositionGuard(this,EditPositionGuard::ANCHOREQUAL |EditPositionGuard::ANCHORBIGGER,
                                                                           EditPositionGuard::SMALLER)));

  readonly->addTransition(new KeyEventTransition(QEvent::KeyPress,
                                                  Qt::Key_Backspace,
                                                  Qt::NoModifier,
                                                  readwrite_state_,
                                                  true));

  readonly->addTransition(new KeyEventTransition(QEvent::KeyPress,
                                                  Qt::Key_Any,
                                                  Qt::NoModifier,
                                                  readwrite_state_,
                                                  false));

  readwrite_state_->addTransition(new SignalTransition(this,
                                                      SIGNAL(cursorPositionChanged()),
                                                      readonly,
                                                      new EditPositionGuard(this,EditPositionGuard::SMALLER | EditPositionGuard::EQUAL,
                                                                                 EditPositionGuard::ANCHORSMALLER)));
  readwrite_state_->addTransition(new SignalTransition(this,
                                                      SIGNAL(cursorPositionChanged()),
                                                      readonly,
                                                      new EditPositionGuard(this,EditPositionGuard::ANCHORSMALLER | EditPositionGuard::ANCHOREQUAL,
                                                                                 EditPositionGuard::SMALLER)));

  readwrite_state_->addTransition(new SignalTransition(this,
                                                        SIGNAL(cursorPositionChanged()),
                                                        mixed,
                                                        new EditPositionGuard(this,EditPositionGuard::EQUAL |EditPositionGuard::BIGGER,
                                                                                   EditPositionGuard::ANCHORSMALLER)));
  readwrite_state_->addTransition(new SignalTransition(this,
                                                        SIGNAL(cursorPositionChanged()),
                                                        mixed,
                                                        new EditPositionGuard(this,EditPositionGuard::ANCHOREQUAL |EditPositionGuard::ANCHORBIGGER,
                                                                                   EditPositionGuard::SMALLER)));

  readwrite_state_->addTransition(new KeyEventTransition(QEvent::KeyPress,
                                                         Qt::Key_Backspace,
                                                         Qt::NoModifier,
                                                         readwrite_state_,
                                                         true,
                                                         new EditPositionGuard(this,EditPositionGuard::EQUAL,
                                                                                    EditPositionGuard::ANCHOREQUAL)));

  mixed->addTransition(new SignalTransition(this,
                                             SIGNAL(cursorPositionChanged()),
                                             readwrite_state_,
                                             new EditPositionGuard(this,EditPositionGuard::EQUAL |EditPositionGuard::BIGGER,
                                                                        EditPositionGuard::ANCHOREQUAL |EditPositionGuard::ANCHORBIGGER)));

  KeyEventTransition* kt1=new KeyEventTransition(QEvent::KeyPress,
                                                 Qt::Key_Backspace,
                                                 Qt::NoModifier,
                                                 readwrite_state_,
                                                 true);
  mixed->addTransition(kt1);
  connect(kt1,SIGNAL(triggered()),this,SLOT(OnMixedToReadwrite()));

  KeyEventTransition* kt2=new KeyEventTransition(QEvent::KeyPress,
                                                 Qt::Key_Any,
                                                 Qt::NoModifier,
                                                 readwrite_state_,
                                                 false);
  mixed->addTransition(kt2);
  connect(kt2,SIGNAL(triggered()),this,SLOT(OnMixedToReadwrite()));

  connect(readonly,SIGNAL(entered()),this,SLOT(OnReadonlyEntered()));
  connect(readwrite_state_,SIGNAL(entered()),this,SLOT(OnReadwriteEntered()));
  readonly_machine_->setInitialState(readwrite_state_);
  readonly_machine_->start();
}

void PythonShellWidget::setup_state_machine_()
{
  #ifdef __APPLE__
    QFlags<Qt::KeyboardModifier> DNG_ARROW_MODIFIERS = Qt::KeypadModifier;
    QFlags<Qt::KeyboardModifier> DNG_ENTER_MODIFIERS = Qt::NoModifier;
  #else
    QFlags<Qt::KeyboardModifier> DNG_ARROW_MODIFIERS = Qt::NoModifier;
    QFlags<Qt::KeyboardModifier> DNG_ENTER_MODIFIERS = Qt::KeypadModifier;
  #endif
  State* single_line=new State;
  State* multi_line_inactive=new State;
  State* completing=new State;
  State* executing=new State;
  State* history_up=new State;
  State* history_down=new State;
  machine_->addState(single_line);
  machine_->addState(multiline_active_state_);
  machine_->addState(multi_line_inactive);
  machine_->addState(completing);
  machine_->addState(executing);
  machine_->addState(history_up);
  machine_->addState(history_down);

  connect(single_line,SIGNAL(entered()),this,SLOT(OnSingleLineStateEntered()));
  connect(multiline_active_state_,SIGNAL(entered()),this,SLOT(OnMultiLineActiveStateEntered()));
  connect(multi_line_inactive,SIGNAL(entered()),this,SLOT(OnMultiLineInactiveStateEntered()));
  connect(history_up,SIGNAL(entered()),this,SLOT(OnHistoryUpStateEntered()));
  connect(history_down,SIGNAL(entered()),this,SLOT(OnHistoryDownStateEntered()));
  connect(executing,SIGNAL(entered()),this,SLOT(OnExecuteStateEntered()));

  KeyEventTransition* tr1=new KeyEventTransition(QEvent::KeyPress,
                                                 Qt::Key_Return,
                                                 Qt::NoModifier,
                                                 executing,
                                                 true,
                                                 new BlockStatusGuard(this,CODE_BLOCK_COMPLETE | CODE_BLOCK_ERROR));
  single_line->addTransition(tr1);
  KeyEventTransition* tr3=new KeyEventTransition(QEvent::KeyPress,
                                                 Qt::Key_Return,
                                                 Qt::NoModifier,
                                                 multiline_active_state_,
                                                 true,
                                                 new BlockStatusGuard(this,CODE_BLOCK_INCOMPLETE));
  single_line->addTransition(tr3);
  KeyEventTransition* keypad_enter1=new KeyEventTransition(QEvent::KeyPress,
                                                           Qt::Key_Enter,
                                                           DNG_ENTER_MODIFIERS| Qt::NoModifier,
                                                           multiline_active_state_,
                                                           true);
  single_line->addTransition(keypad_enter1);
  connect(keypad_enter1,SIGNAL(triggered()),this,SLOT(OnKeypadEnterTransition()));
  KeyEventTransition* keypad_enter2=new KeyEventTransition(QEvent::KeyPress,
                                                           Qt::Key_Enter,
                                                           DNG_ENTER_MODIFIERS| Qt::ShiftModifier,
                                                           multiline_active_state_,
                                                           true);
  single_line->addTransition(keypad_enter2);
  connect(keypad_enter2,SIGNAL(triggered()),this,SLOT(OnKeypadEnterTransition()));
  KeyEventTransition* keypad_enter3=new KeyEventTransition(QEvent::KeyPress,
                                                           Qt::Key_Enter,
                                                           DNG_ENTER_MODIFIERS| Qt::MetaModifier,
                                                           multiline_active_state_,
                                                           true);
  single_line->addTransition(keypad_enter3);
  connect(keypad_enter3,SIGNAL(triggered()),this,SLOT(OnKeypadEnterTransition()));
  
  single_line->addTransition(new KeyEventTransition(QEvent::KeyPress,
                                                    Qt::Key_Return,
                                                    Qt::ControlModifier,
                                                    multiline_active_state_,
                                                    false));
  // just to make OSX happy we also add the transitions with Meta (-> Ctrl o OSX)
  single_line->addTransition(new KeyEventTransition(QEvent::KeyPress,
                                                    Qt::Key_Return,
                                                    Qt::MetaModifier,
                                                    multiline_active_state_,
                                                    false));
                                                 
  connect(tr3,SIGNAL(triggered()),this,SLOT(OnEnterTransition()));
  single_line->addTransition(new KeyEventTransition(QEvent::KeyPress,Qt::Key_Up,DNG_ARROW_MODIFIERS,history_up));
  single_line->addTransition(new KeyEventTransition(QEvent::KeyPress,Qt::Key_Down,DNG_ARROW_MODIFIERS,history_down));

  KeyEventTransition* tr4=new KeyEventTransition(QEvent::KeyPress,
                                                 Qt::Key_Return,
                                                 Qt::NoModifier,
                                                 executing,
                                                 true,
                                                 new BlockStatusGuard(this,CODE_BLOCK_COMPLETE | CODE_BLOCK_ERROR));
  multi_line_inactive->addTransition(tr4);
  KeyEventTransition* tr6=new KeyEventTransition(QEvent::KeyPress,
                                                 Qt::Key_Return,
                                                 Qt::NoModifier,
                                                 multiline_active_state_,
                                                 true,
                                                 new BlockStatusGuard(this,CODE_BLOCK_INCOMPLETE));
  multi_line_inactive->addTransition(tr6);
  connect(tr6,SIGNAL(triggered()),this,SLOT(OnEnterTransition()));
  multi_line_inactive->addTransition(new KeyEventTransition(QEvent::KeyPress,Qt::Key_Left,DNG_ARROW_MODIFIERS,multiline_active_state_));
  multi_line_inactive->addTransition(new KeyEventTransition(QEvent::KeyPress,Qt::Key_Right,DNG_ARROW_MODIFIERS,multiline_active_state_));
  multi_line_inactive->addTransition(new KeyEventTransition(QEvent::KeyPress,Qt::Key_Return,Qt::ControlModifier,multiline_active_state_));
  multi_line_inactive->addTransition(new KeyEventTransition(QEvent::KeyPress,Qt::Key_Up,DNG_ARROW_MODIFIERS,history_up));
  multi_line_inactive->addTransition(new KeyEventTransition(QEvent::KeyPress,Qt::Key_Down,DNG_ARROW_MODIFIERS,history_down));
  KeyEventTransition* keypad_enter4=new KeyEventTransition(QEvent::KeyPress,
                                                           Qt::Key_Enter,
                                                           DNG_ENTER_MODIFIERS| Qt::NoModifier,
                                                           multiline_active_state_,
                                                           true);
  multi_line_inactive->addTransition(keypad_enter4);
  connect(keypad_enter4,SIGNAL(triggered()),this,SLOT(OnKeypadEnterTransition()));
  KeyEventTransition* keypad_enter5=new KeyEventTransition(QEvent::KeyPress,
                                                           Qt::Key_Enter,
                                                           DNG_ENTER_MODIFIERS| Qt::ShiftModifier,
                                                           multiline_active_state_,
                                                           true);
  multi_line_inactive->addTransition(keypad_enter5);
  connect(keypad_enter5,SIGNAL(triggered()),this,SLOT(OnKeypadEnterTransition()));
  KeyEventTransition* keypad_enter6=new KeyEventTransition(QEvent::KeyPress,
                                                           Qt::Key_Enter,
                                                           DNG_ENTER_MODIFIERS | Qt::MetaModifier,
                                                           multiline_active_state_,
                                                           true);
  multi_line_inactive->addTransition(keypad_enter6);
  connect(keypad_enter6,SIGNAL(triggered()),this,SLOT(OnKeypadEnterTransition()));
  multi_line_inactive->addTransition(new KeyEventTransition(QEvent::KeyPress,
                                                            Qt::Key_Return,
                                                            Qt::ControlModifier,
                                                            multiline_active_state_,
                                                            false));
  // just to make OSX happy we also add the transitions with Meta (-> Ctrl o OSX)
  multi_line_inactive->addTransition(new KeyEventTransition(QEvent::KeyPress,
                                                            Qt::Key_Return,
                                                            Qt::MetaModifier,
                                                            multiline_active_state_,
                                                            false));

  KeyEventTransition* tr7=new KeyEventTransition(QEvent::KeyPress,
                                                 Qt::Key_Return,
                                                 Qt::NoModifier,
                                                 executing,
                                                 true,
                                                 new BlockStatusGuard(this,CODE_BLOCK_COMPLETE | CODE_BLOCK_ERROR));
  multiline_active_state_->addTransition(tr7);
  KeyEventTransition* tr8=new KeyEventTransition(QEvent::KeyPress,
                                                 Qt::Key_Return,
                                                 Qt::NoModifier,
                                                 multiline_active_state_,
                                                 true,
                                                 new BlockStatusGuard(this,CODE_BLOCK_INCOMPLETE));
  multiline_active_state_->addTransition(tr8);
  connect(tr8,SIGNAL(triggered()),this,SLOT(OnEnterTransition()));

  multiline_active_state_->addTransition(new KeyEventTransition(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier,multi_line_inactive));
  multiline_active_state_->addTransition(new KeyEventTransition(QEvent::KeyPress,Qt::Key_Up,Qt::ControlModifier | DNG_ARROW_MODIFIERS,history_up));
  multiline_active_state_->addTransition(new KeyEventTransition(QEvent::KeyPress,Qt::Key_Down,Qt::ControlModifier | DNG_ARROW_MODIFIERS,history_down));
  KeyEventTransition* keypad_enter7=new KeyEventTransition(QEvent::KeyPress,
                                                           Qt::Key_Enter,
                                                           DNG_ENTER_MODIFIERS | Qt::NoModifier,
                                                           multiline_active_state_,
                                                           true);
  multiline_active_state_->addTransition(keypad_enter7);
  connect(keypad_enter7,SIGNAL(triggered()),this,SLOT(OnKeypadEnterTransition()));
  KeyEventTransition* keypad_enter8=new KeyEventTransition(QEvent::KeyPress,
                                                           Qt::Key_Enter,
                                                           DNG_ENTER_MODIFIERS | Qt::ShiftModifier,
                                                           multiline_active_state_,
                                                           true);
  multiline_active_state_->addTransition(keypad_enter8);
  connect(keypad_enter8,SIGNAL(triggered()),this,SLOT(OnKeypadEnterTransition()));
  KeyEventTransition* keypad_enter9=new KeyEventTransition(QEvent::KeyPress,
                                                           Qt::Key_Enter,
                                                           DNG_ENTER_MODIFIERS | Qt::MetaModifier,
                                                           multiline_active_state_,
                                                           true);
  multiline_active_state_->addTransition(keypad_enter9);
  connect(keypad_enter9,SIGNAL(triggered()),this,SLOT(OnKeypadEnterTransition()));

  history_up->addTransition(new AutomaticTransition(multi_line_inactive,new HistoryGuard(&history_,EDITMODE_MULTILINE_INACTIVE)));
  history_up->addTransition(new AutomaticTransition(single_line,new HistoryGuard(&history_,EDITMODE_SINGLELINE)));
  history_down->addTransition(new AutomaticTransition(multi_line_inactive,new HistoryGuard(&history_,EDITMODE_MULTILINE_INACTIVE)));
  history_down->addTransition(new AutomaticTransition(single_line,new HistoryGuard(&history_,EDITMODE_SINGLELINE)));

  executing->addTransition(new AutomaticTransition(single_line));

  machine_->setInitialState(single_line);
  machine_->start();
}

void PythonShellWidget::OnReadonlyEntered()
{
  setReadOnly(true);
  setTextInteractionFlags(textInteractionFlags() | Qt::TextSelectableByKeyboard);
}

void PythonShellWidget::OnReadwriteEntered()
{
  if(textCursor().position()< GetEditStartBlock().position()){
    moveCursor(QTextCursor::End);
  }
  setReadOnly(false);
}
void PythonShellWidget::OnMixedToReadwrite()
{
  QTextCursor tc=textCursor();
  if(tc.position()< GetEditStartBlock().position()){
    tc.setPosition(GetEditStartBlock().position(),QTextCursor::KeepAnchor);
    setTextCursor(tc);
  }else{
    tc.setPosition(GetEditStartBlock().position(),QTextCursor::MoveAnchor);
    tc.setPosition(textCursor().position(),QTextCursor::KeepAnchor);
    setTextCursor(tc);
  }

}

void PythonShellWidget::OnKeypadEnterTransition()
{
  insertPlainText(QString(QChar::LineSeparator));
}
void PythonShellWidget::OnEnterTransition()
{
  QTextCursor cursor=textCursor();
  cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::MoveAnchor);
  setTextCursor(cursor);
  cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor);
  bool new_indent=cursor.selectedText()==":";
  cursor.movePosition(QTextCursor::StartOfLine);
  cursor.movePosition(QTextCursor::NextWord, QTextCursor::KeepAnchor);
  insertPlainText(QString(QChar::ParagraphSeparator));
  if(cursor.selectedText()[0].isSpace() && ! cursor.atBlockEnd()){
    insertPlainText(QString(cursor.selectedText()));
  }
  if (new_indent){
    insertPlainText(QString("\t"));
  }
}
void PythonShellWidget::OnSingleLineStateEntered()
{
  set_block_edit_mode_(EDITMODE_SINGLELINE);
}
void PythonShellWidget::OnMultiLineActiveStateEntered()
{
    set_block_edit_mode_(EDITMODE_MULTILINE_ACTIVE);
}
void PythonShellWidget::OnMultiLineInactiveStateEntered()
{
  set_block_edit_mode_(EDITMODE_MULTILINE_INACTIVE);
}
void PythonShellWidget::OnHistoryUpStateEntered()
{
  if(history_.AtEnd()){
    history_.SetCurrentCommand(GetCommand(),get_block_edit_mode_());
  }
  history_.MoveToPreviousMatch();
  set_command_(history_.GetCommand());
  set_block_edit_mode_(history_.GetCommandMode());
}
void PythonShellWidget::OnHistoryDownStateEntered()
{
  history_.MoveToNextMatch();
  set_command_(history_.GetCommand());
  set_block_edit_mode_(history_.GetCommandMode());
}
void PythonShellWidget::OnExecuteStateEntered()
{
  QTextCursor cursor=textCursor();
  cursor.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
  setTextCursor(cursor);
  set_block_type_(block_edit_start_,textCursor().block(),BLOCKTYPE_CODE);
  insertPlainText(QString(QChar::ParagraphSeparator));
  QString command=GetCommand();
  QString command_trimmed=command.trimmed();
  if (command_trimmed.size()>0) {
    unsigned int id=PythonInterpreter::Instance().RunCommand(command);
    output_blocks_.insert(id,textCursor().block());
    history_.AppendCommand(command_trimmed,get_block_edit_mode_());
    insertPlainText(QString(QChar::ParagraphSeparator));
  }
  block_edit_start_=textCursor().block();
  std::cout << block_edit_start_.isValid() << std::endl;
}


void PythonShellWidget::SetTabWidth(int width) {
  tab_width_=width;
  QFontMetrics metrics(font());
  setTabStopWidth(tab_width_*metrics.width(" "));
}



int PythonShellWidget::GetTabWidth() const {
  return tab_width_;
}



void PythonShellWidget::wrap_into_function_(const QString& command)
{
  QString tmp_command=command;
  tmp_command.replace(QString(QChar::LineSeparator),
                      QString(QChar::LineSeparator)+QString("\t"));
  QString tmp="def func():"+QString(QChar::LineSeparator)+"\t";
  tmp+=tmp_command;
  set_command_(tmp);
  QTextCursor tc=textCursor();
  tc.setPosition(document()->lastBlock().position()+QString("def ").length());
  tc.setPosition(document()->lastBlock().position()+
                 QString("def func").length(),QTextCursor::KeepAnchor);
  setTextCursor(tc);
}

void PythonShellWidget::Recomplete(const QString& completion)
{
  InsertCompletion(completion);
  Complete(false);
}

void  PythonShellWidget::InsertCompletion(const QString& completion)
{
  QTextCursor tc = textCursor();
  tc.setPosition(completion_start_);
  tc.setPosition(completion_end_,QTextCursor::KeepAnchor);
  tc.insertText(completion);
  setTextCursor(tc);
}
void  PythonShellWidget::InsertPathCompletion(const QString& completion)
{
  QString path=QDir::fromNativeSeparators(completion);
  // append dir separator for directories if none present (Windows adds it already for the inline completion)
  if(QFileInfo(path).isDir() && ! completion.endsWith("/")){
    path+="/";
  }
  InsertCompletion(path);
}


void PythonShellWidget::AppendOutput(unsigned int id,const QString& output)
{
  OutputBlockList::iterator it=output_blocks_.find(id);
  if(it==output_blocks_.end()){
    return;
  }
  if(! it->isValid()){
    return;
  }
  QTextCursor cursor(*it);
  cursor.movePosition(QTextCursor::EndOfBlock);
  cursor.insertText(output);
  set_block_type_(output_blocks_[id],cursor.block(),BLOCKTYPE_OUTPUT);
  output_blocks_[id]=cursor.block();
  verticalScrollBar()->triggerAction(QAbstractSlider::SliderToMaximum);
  ensureCursorVisible();
  repaint();
  QApplication::processEvents();
}

void PythonShellWidget::AppendError(unsigned int id,const QString& output)
{
  OutputBlockList::iterator it=output_blocks_.find(id);
  if(it==output_blocks_.end()){
    return;
  }
  if(! it->isValid()){
    return;
  }
  QTextCursor cursor(*it);
  cursor.movePosition(QTextCursor::EndOfBlock);
  cursor.insertText(output);
  set_block_type_(output_blocks_[id],cursor.block(),BLOCKTYPE_ERROR);
  output_blocks_[id]=cursor.block();
  verticalScrollBar()->triggerAction(QAbstractSlider::SliderToMaximum);
  ensureCursorVisible();
  repaint();
  QApplication::processEvents();
}

void PythonShellWidget::set_block_type_(const QTextBlock& start, const QTextBlock& end, BlockType type)
{
  QTextBlock block=start;
  while(block!=end){
    block.setUserState(type);
    block=block.next();
  }
  block.setUserState(type);
  document()->markContentsDirty(start.position(),end.position()+end.length()-start.position());
}

void PythonShellWidget::OutputFinished(unsigned int id, bool error)
{
  OutputBlockList::iterator it=output_blocks_.find(id);
  if(it==output_blocks_.end()){
    return;
  }
  if(it->isValid()){
    QTextCursor cursor(*it);
    cursor.movePosition(QTextCursor::Left,QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
  }
  output_blocks_.erase(it);
}

void PythonShellWidget::set_command_(const QString& command)
{
  QTextCursor cursor(block_edit_start_);
  cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
  cursor.removeSelectedText();
  if(block_edit_start_.isValid()){
    block_edit_start_=document()->lastBlock();
  }
  cursor.insertText(command);
  QAbstractTextDocumentLayout* layout=document()->documentLayout();
  dynamic_cast<PythonShellTextDocumentLayout*>(layout)->EmitSizeChange();
  verticalScrollBar()->triggerAction(QAbstractSlider::SliderToMaximum);
  ensureCursorVisible();
}


void PythonShellWidget::Complete(bool inline_completion)
{
  QTextCursor cursor=textCursor();
  if(cursor.position()<document()->lastBlock().position()){
    return;
  }
  StringLiteralPositions* positions=dynamic_cast<StringLiteralPositions*>(document()->lastBlock().userData());
  int literal_index=-1;
  if(positions){
    literal_index=positions->GetStringLiteralIndex(textCursor().position());
  }
  if (literal_index>=0) {
    cursor.setPosition(positions->GetStart(literal_index));
    cursor.setPosition(positions->GetEnd(literal_index), 
                       QTextCursor::KeepAnchor);
    completion_start_=positions->GetStart(literal_index);
    completion_end_=positions->GetEnd(literal_index);
    QTextCursor cp=textCursor();
    QString text=cursor.selectedText();        
    int sep=text.lastIndexOf("/");    
    cp.setPosition(positions->GetStart(literal_index));
    if (sep!=-1) {
      cp.movePosition(QTextCursor::NextCharacter,
                      QTextCursor::MoveAnchor, sep);      
    }
    if (QDir(text).isAbsolute()) {
      emit SetPathCompletionPrefix(text);
    } else {
      emit SetPathCompletionPrefix(QDir::currentPath()+"/"+text);
    }
    emit RequestPathCompletion(cursorRect(cp), 
                               inline_completion);
  } else {
    cursor.clearSelection();
    cursor.movePosition(QTextCursor::StartOfLine, QTextCursor::KeepAnchor);
    QString command=cursor.selectedText();
    QRegExp rx("[\\w,\\.]*$");
    int index=rx.indexIn(command);
    completion_start_=cursor.position()+index;
    completion_end_=cursor.position()+index+rx.cap().size();
    emit SetCompletionPrefix(rx.cap());
    // determine position for completion popup. This is basically the position 
    // of the first character after a dot or the beginning of the completion
    // prefix if there is no dot.
    QTextCursor cp=textCursor();    
    int dot=command.lastIndexOf('.');
    int left_shift=rx.cap().size();
    if (dot>=index) {
      left_shift=rx.cap().size()-(dot-index);
    }
    cp.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor,
                    left_shift);
    emit RequestCompletion(cursorRect(cp), inline_completion);
  }
}



bool PythonShellWidget::handle_custom_commands_(QKeyEvent* event)
{
  if ((event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier)) && 
      event->key() == Qt::Key_H) {
    set_output_visible_(!output_visible_);
    return true;
  }

  if ((event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier)) && 
      event->key() == Qt::Key_W) {
    wrap_into_function_(textCursor().selectedText());
    return true;
  }
  // custom handling of CTRL+A to select only text in edit or output section
  if (event->modifiers() == Qt::ControlModifier && 
      event->key() == Qt::Key_A) {
    QTextCursor cursor=textCursor();
    if(cursor.position()<GetEditStartBlock().position()){
      // select all output
      cursor.setPosition(0);
      cursor.setPosition(GetEditStartBlock().position(),QTextCursor::KeepAnchor);
      setTextCursor(cursor);
    }else{
      //select whole edit section
      cursor.setPosition(GetEditStartBlock().position());
      cursor.movePosition(QTextCursor::End,QTextCursor::KeepAnchor);
      setTextCursor(cursor);
    }
    return true;
  }
  return false;
}


bool PythonShellWidget::handle_completion_(QKeyEvent* event)
{
  if(event->key() == Qt::Key_Tab){
    QRegExp rx("^\\s*$"); // only white spaces from beginning of line
    QTextCursor cursor= textCursor();
    cursor.clearSelection();
    cursor.movePosition(QTextCursor::StartOfLine, QTextCursor::KeepAnchor);
    QString command=cursor.selectedText();
    if(!command.contains(rx)){
      Complete();
      return true;
    }
  }  
  return false;
}

QTextBlock PythonShellWidget::GetEditStartBlock()
{
  return block_edit_start_;
}


void PythonShellWidget::keyPressEvent(QKeyEvent* event)
{
  // BZDNG-238
  // Letting Qt do the handling of the backspace key leads to a crash when
  // multiline block mode and doing the following:
  // 
  //   (a) Hit Ctrl+A
  //   (b) Hit Backspace
  //   (c) Hit Return
  //
  // If we emulate the deletion of the text manually all is fine.
  if (event->key()==Qt::Key_Backspace) {
    QTextCursor cursor=this->textCursor();
    if (cursor.hasSelection()) {
      cursor.removeSelectedText();
    } else {
      if (cursor.position()>this->GetEditStartBlock().position()) {
        cursor.deletePreviousChar();
      }
    }
    event->accept();
    return;
  }
  // BZDNG-173
  if (event->key()==Qt::Key_Left) {
    if (this->textCursor().position()==GetEditStartBlock().position() ||
        this->textCursor().anchor()==GetEditStartBlock().position()) {
      event->accept();
      return;
    }
  }
  if (this->handle_custom_commands_(event)){
    return;
  }
  if (this->handle_completion_(event)){
    return;
  }
  QPlainTextEdit::keyPressEvent(event);
}


void PythonShellWidget::mouseReleaseEvent(QMouseEvent * event)
{
  QTextCursor mouse_cursor=cursorForPosition(event->pos());
  if (get_block_edit_mode_()==EDITMODE_MULTILINE_INACTIVE &&
      event->button() == Qt::LeftButton &&
      mouse_cursor.position()>=document()->lastBlock().position()) {
    // switch to active edit mode upon mouse click in edit section
    set_block_edit_mode_(EDITMODE_MULTILINE_ACTIVE);
  }
  if (event->button() == Qt::MidButton && 
      mouse_cursor.position()<document()->lastBlock().position()) {
    // fix cursor position before inserting text
    setTextCursor(mouse_cursor);
    return;
  }
  QPlainTextEdit::mouseReleaseEvent(event);
}



void PythonShellWidget::set_block_edit_mode_(BlockEditMode flag)
{
  block_edit_mode_=flag;
  if(flag==EDITMODE_MULTILINE_ACTIVE){
    set_block_type_(block_edit_start_,document()->lastBlock(),BLOCKTYPE_BLOCKEDIT);
  }else if(flag==EDITMODE_MULTILINE_INACTIVE){
    set_block_type_(block_edit_start_,document()->lastBlock(),BLOCKTYPE_ACTIVE);
  }else {
    set_block_type_(block_edit_start_,document()->lastBlock(),BLOCKTYPE_ACTIVE);
  }
  gutter_->update();
}

BlockEditMode PythonShellWidget::get_block_edit_mode_()
{
  return block_edit_mode_;
}

QString PythonShellWidget::GetCommand()
{

  QTextCursor cursor(block_edit_start_);
  cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
  QString text= cursor.selectedText();
  // replace LineSeparator with an empty string, they are used by Qt to 
  // designate soft line wraps
  text.replace(QChar::LineSeparator,"");
  // a real line end
  text.replace(QChar::ParagraphSeparator,"\n");
  return text;
}

void PythonShellWidget::insertFromMimeData(const QMimeData * source)
{
  if(!source->hasText()){
    return;
  }
  QString text=source->text();
  readonly_machine_->setActive(readwrite_state_);
  text=text.replace("\r\n", QString(1, QChar::LineSeparator))
           .replace('\n', QChar::LineSeparator)
           .replace('\r', QChar::LineSeparator);
  // replace spaces at the beginning of lines by tabs, because spaces are evil!
  QStringList lines=text.split(QChar::LineSeparator);
  for (int i=0; i <lines.size(); ++i) {
    int num_spaces=0;
    for (int j=0; j<lines[i].size()-1; j+=2) {
      if (lines[i][j]==' ' && lines[i][j+1]==' ') {
        num_spaces+=1;
      } else {
        break;
      }
    }
    QString right=lines[i].right(lines[i].size()-2*num_spaces);
    lines[i]=QString(num_spaces, '\t')+right;
  }
  text=lines.join(QString(1, QChar::LineSeparator));
  if(lines.size()>0){
    machine_->setActive(multiline_active_state_);
  }
  QPlainTextEdit::insertFromMimeData(source);
  if(lines.size()>0){
    set_block_type_(block_edit_start_,document()->lastBlock(),BLOCKTYPE_BLOCKEDIT);
  }
}

GutterBlockList PythonShellWidget::GetGutterBlocks(const QRect& rect)
{
  GutterBlockList result;
  for (QTextBlock i=firstVisibleBlock(); i!=document()->end(); i=i.next()) {
    QRectF bounds=blockBoundingGeometry(i);
    QRectF geom=bounds.translated(contentOffset());
    if (geom.top()<=rect.bottom() && (geom.top()>=rect.top() || 
        geom.bottom() <= rect.bottom() || (geom.top() <= rect.top() && 
        geom.bottom() >= rect.bottom())) ) {
      BlockType bt=static_cast<BlockType>(i.userState());
      result.push_back(GutterBlock(geom.top(),geom.bottom(),bt));
    } else {
      break;
    }
  }
  return result;
}

void PythonShellWidget::set_output_visible_(bool flag)
{
  output_visible_=flag;
  for (QTextBlock i=document()->begin(); i!=document()->end(); i=i.next()) {
    if(i.userState()==BLOCKTYPE_ERROR || i.userState()==BLOCKTYPE_OUTPUT){
      i.setVisible(flag);
      i.setLineCount(flag ? qMax(1, i.layout()->lineCount()) : 0);
    }
  }
  dynamic_cast<PythonShellTextDocumentLayout*>(document()->documentLayout())->requestUpdate();
  dynamic_cast<PythonShellTextDocumentLayout*>(document()->documentLayout())->EmitSizeChange();
  ensureCursorVisible();
  repaint();
  gutter_->update();
  viewport()->update();
  this->update();
}

void PythonShellWidget::resizeEvent(QResizeEvent* event)
{
  QPlainTextEdit::resizeEvent(event);
  QRect cr=contentsRect();
  gutter_->setGeometry(QRect(cr.left(), cr.top(),Gutter::GUTTER_WIDTH, 
                       cr.height()));
}

void PythonShellWidget::showEvent(QShowEvent* event)
{
  static bool init=false;
  if(!init) {
    init=true;
  }
}


}}
