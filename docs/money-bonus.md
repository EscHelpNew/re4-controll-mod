# Разовый бонус песет / One-time peseta bonus

С версии **0.27.7** кнопка **«Игра → Получить 200 000 песет»** не зависит от
слота, CRC или этапа прохождения. Загрузите основную историю, выберите кнопку
и подтвердите предупреждение. Расширенная прокачка оружия на выдачу не влияет.

Отметка хранится отдельно на SD:
`config/re4-controls/money-bonus.bin`. Пока файл существует, повторная выдача
заблокирована. Загрузка другого сохранения, перезапуск и новая игра эту отметку
не сбрасывают. Файл создаётся при подтверждённой выдаче; обычное сохранение игры
не создаёт и не переписывает его.

После получения сохраните игру: сами деньги сохраняются обычным игровым
сохранением. Закрытие или загрузка игры до сохранения может потерять деньги,
а отметка получения останется. При ошибке записи деньги не добавляются.

Успешное получение в старой 0.27.6 переносится в новый файл. Старые журналы без
отметки получения и неизвестные им сохранения не мешают выдаче в середине игры.
Ошибки чтения SD показываются как ошибка хранения, а не как разрешение получить
бонус повторно. Повреждённая старая история не используется для регистрации слотов.

Starting with **0.27.7**, **Game → Receive 200 000 pesetas** uses one separate SD
receipt instead of native save identities. Any loaded main-story save can receive
exactly 200,000 after confirmation if `config/re4-controls/money-bonus.bin` is
absent. Any existing receipt blocks another grant, including after a restart,
loading another save or New Game. The native wallet limit remains 99,999,999.

Save the game after receiving money; the action does not autosave. A valid positive
0.27.6 receipt is migrated, while its unclaimed slot history no longer blocks old
or mid-game saves. Storage I/O errors never authorize another grant. Keep the
receipt file when transferring the SD installation.
