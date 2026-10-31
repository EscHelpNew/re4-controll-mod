#define TESLA_INIT_IMPL
#include <tesla.hpp>
#include "worker.hpp"
#include <cstring>

namespace {
telemetry::Worker worker;
telemetry::Target target=telemetry::Target::RE6;
bool russian=false,storage_ready=false,language_save_failed=false;
const char* tr(const char* en,const char* ru){return russian?ru:en;}
const char* error_text(re6::RecorderError error){
 switch(error){
  case re6::RecorderError::Attach:return tr("Journal attach failed","Ошибка подключения журнала");
  case re6::RecorderError::Open:return tr("Could not create file on SD","Не удалось создать файл на SD");
  case re6::RecorderError::Write:return tr("SD write failed; partial file retained","Ошибка записи SD; сохранён .part");
  case re6::RecorderError::Finalize:return tr("Could not finalize file; retained .part","Ошибка сохранения; остался .part");
  case re6::RecorderError::Changed:return tr("Game restarted; partial file retained","Игра перезапущена; остался .part");
  case re6::RecorderError::Read:return tr("Game read failed; partial file retained","Ошибка чтения игры; остался .part");
  case re6::RecorderError::Stalled:return tr("Journal unstable; partial file retained","Журнал нестабилен; остался .part");
  default:return "";
 }
}
const char* detail_text(const char* detail){
 struct Pair{const char* en;const char* ru;};
 static constexpr Pair messages[]={
  {"Debug service unavailable","Сервис отладки недоступен"},
  {"Start the selected game; attach failed","Запустите выбранную игру"},
  {"Unsupported game or build ID","Игра или версия не поддерживается"},
  {"Memory map query failed","Не удалось прочитать карту памяти"},
  {"Memory scan limit reached","Достигнут предел поиска памяти"},
  {"Start cancelled","Начало записи отменено"},
  {"Journal not found; install module and restart game","Журнал не найден. Установите модуль"},
  {"Could not create recorder worker","Не удалось создать поток записи"},
  {"Could not start recorder worker","Не удалось запустить поток записи"},
  {"Looking for native journal","Поиск журнала игрового модуля"},
  {"Recording continues when overlay is hidden","Запись идёт и при скрытом оверлее"},
  {"Could not start recording","Не удалось начать запись"},
  {"Recording saved","Запись сохранена"},
  {"Recording failed; partial file retained","Ошибка записи; сохранён файл .part"}
 };
 if(russian)for(const auto& message:messages)if(!std::strcmp(message.en,detail))return message.ru;
 return detail;
}
const char* status_text(const telemetry::Snapshot& state){
 if(!storage_ready)return tr("Could not mount SD card","Не удалось подключить SD");
 if(state.busy)return tr("Finding journal...","Поиск журнала...");
 if(state.error==re6::RecorderError::Attach&&state.detail[0])return detail_text(state.detail);
 if(state.error!=re6::RecorderError::None)return error_text(state.error);
 switch(state.state){
  case re6::RecorderState::Recording:return tr("Recording (also while hidden)","Запись (и при скрытом оверлее)");
  case re6::RecorderState::Stopping:return tr("Draining snapshot and saving...","Завершение записи и сохранение...");
  case re6::RecorderState::Saved:return tr("Saved","Сохранено");
  default:return state.detail[0]?detail_text(state.detail):tr("Ready; press Start recording","Готово; нажмите «Начать запись»");
 }
}
class Frame : public tsl::elm::OverlayFrame {
public:
 Frame():OverlayFrame("RE4 / RE6 Telemetry",""){}
 void layout(u16 x,u16 y,u16 width,u16 height)override{
  OverlayFrame::layout(x,y,width,height);
  if(m_contentElement){m_contentElement->setBoundaries(x+35,y+125,width-85,305);m_contentElement->invalidate();}
 }
 void draw(tsl::gfx::Renderer* renderer)override{
  OverlayFrame::draw(renderer);const auto state=worker.snapshot();const auto color=a(tsl::style::color::ColorDescription);
  renderer->drawString(status_text(state),false,22,465,15,color);
  char text[128];std::snprintf(text,sizeof(text),tr("Records: %llu  Drops: %llu","Записей: %llu  Потерь: %llu"),
   (unsigned long long)state.records,(unsigned long long)state.dropped);
  renderer->drawString(text,false,22,492,16,color);
  std::snprintf(text,sizeof(text),tr("Errors: %u  IPC: %08X","Ошибок: %u  IPC: %08X"),state.errors,state.result);
  renderer->drawString(text,false,22,516,15,color);
  if(state.diagnostic_failed)renderer->drawString(tr("Could not save error report to SD","Не удалось сохранить отчёт об ошибке на SD"),false,22,538,13,color);
  renderer->drawString(tr("File (sdmc:/switch/re4-controls/telemetry):","Файл (sdmc:/switch/re4-controls/telemetry):"),false,22,548,13,color);
  const auto length=std::strlen(state.filename);char line[40]{};
  std::memcpy(line,state.filename,std::min<std::size_t>(length,38));renderer->drawString(line,false,22,571,14,color);
  if(length>38)renderer->drawString(state.filename+38,false,22,592,14,color);
  renderer->drawString(language_save_failed?tr("Language preference could not be saved","Не удалось сохранить язык"):tr("B / Tesla combo: hide, keep recording","B / комбинация Tesla: скрыть, запись идёт"),false,22,631,14,color);
  renderer->drawString(tr("Raw input + native player / camera matrices","Ввод + матрицы игрока и камеры игры"),false,22,655,14,color);
 }
};
class Gui : public tsl::Gui {
 Frame* frame_=nullptr;
 tsl::elm::ListItem *game_=nullptr,*start_=nullptr,*stop_=nullptr,*language_=nullptr,*exit_=nullptr;
public:
 tsl::elm::Element* createUI()override{
  frame_=new Frame;auto* list=new tsl::elm::List;
  game_=new tsl::elm::ListItem("Game");
  game_->setClickListener([](u64 keys){if(!(keys&(HidNpadButton_A|HidNpadButton_Left|HidNpadButton_Right)))return false;const auto s=worker.snapshot();if(!s.busy&&s.state!=re6::RecorderState::Recording&&s.state!=re6::RecorderState::Stopping)target=target==telemetry::Target::RE6?telemetry::Target::RE4:telemetry::Target::RE6;return true;});
  start_=new tsl::elm::ListItem("Start recording","A");
  start_->setClickListener([](u64 keys){if(!(keys&HidNpadButton_A))return false;if(storage_ready)worker.start(target);return true;});
  stop_=new tsl::elm::ListItem("Stop and save","A");
  stop_->setClickListener([](u64 keys){if(!(keys&HidNpadButton_A))return false;worker.stop();return true;});
  language_=new tsl::elm::ListItem("Language");
  language_->setClickListener([](u64 keys){
   if(!(keys&(HidNpadButton_A|HidNpadButton_Left|HidNpadButton_Right)))return false;
   russian=!russian;language_save_failed=true;
   if(storage_ready)if(FILE* file=std::fopen(telemetry::kLanguagePath,"wb")){const bool wrote=std::fputs(russian?"ru\n":"en\n",file)>=0;const bool closed=std::fclose(file)==0;language_save_failed=!(wrote&&closed);}
   return true;
  });
  exit_=new tsl::elm::ListItem("Exit (save recording)","A");
  exit_->setClickListener([](u64 keys){if(!(keys&HidNpadButton_A))return false;tsl::Overlay::get()->close();return true;});
  for(auto* item:{game_,start_,stop_,language_,exit_})list->addItem(item);
  frame_->setContent(list);update();return frame_;
 }
 void update()override{
  frame_->setSubtitle(tr("Read-only frame journal","Покадровый журнал — только чтение"));
  game_->setText(tr("Game","Игра"));game_->setValue(target==telemetry::Target::RE6?"Resident Evil 6":"Resident Evil 4");
  start_->setText(tr("Start recording","Начать запись"));stop_->setText(tr("Stop and save","Остановить и сохранить"));
  language_->setText(tr("Language","Язык"));language_->setValue(russian?"Русский":"English");exit_->setText(tr("Exit (save recording)","Выход (сохранить запись)"));
 }
};
class Overlay : public tsl::Overlay {
 void initServices()override{
  storage_ready=telemetry::mount_storage();
  if(storage_ready)if(FILE* file=std::fopen(telemetry::kLanguagePath,"rb")){russian=std::fgetc(file)=='r';std::fclose(file);}
  worker.initialize();
 }
 // Hiding never changes the worker. Explicit unload joins the finite Stop
 // snapshot, flushes and closes the file, then unmounts its private SD alias.
 void exitServices()override{worker.close();telemetry::close_storage();}
 std::unique_ptr<tsl::Gui> loadInitialGui()override{return initially<Gui>();}
};
}
int main(int argc,char** argv){return tsl::loop<Overlay,tsl::impl::LaunchFlags::None>(argc,argv);}
