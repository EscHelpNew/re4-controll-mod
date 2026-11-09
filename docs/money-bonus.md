# One-time peseta bonus / Разовый бонус песет

**Game → Receive 200 000 pesetas** / **Игра → Получить 200 000 песет**
opens a warning with Cancel and Receive buttons. It is independent of extended
weapon upgrades. Exactly 200,000 is added to the loaded main-story wallet;
insufficient space under the game's 99,999,999 limit refuses the action.

The claim is recorded on SD before money is added. Save your game afterwards:
closing or reloading before a normal save can lose the unsaved money, while
the consumed claim remains. This action does not autosave the game.

The first existing save is enrolled automatically. Subsequent saves made in
that tracked playthrough, including different slots, retain its identity.
Fresh main-story games and explicit cleared-save New Game Plus launches start
new identities; checkpoints, death, ordinary Continue and overlay reopening do
not reset the claim. Separate Ways and other modes cannot claim it.

RE4 has no native campaign identifier. An older or imported save not previously
observed by this mod is therefore unavailable after initial enrollment, rather
than treated as another free claim. Keep the two `money-bonus-*.bin` files under
`config/re4-controls` when transferring the SD installation. Missing both files
removes the mod's claim history. Corrupt journals block grants. History is
bounded to 256 campaigns and 4096 save identities; a full history blocks new
entries without deleting old claims.

Бонус выдаётся отдельно от расширенной прокачки, после предупреждения и
подтверждения. После получения сохраните игру: несохранённые деньги могут
потеряться, а запрет повторной выдачи останется. Перезапуск игры и переход в
другой отслеженный слот не сбрасывают запрет. Новая основная игра и запуск
New Game Plus из пройденного сохранения начинают новое прохождение.

Старые или перенесённые сохранения без известной модификации истории могут
показывать «Недоступно»: игра не позволяет надёжно отличить другое прохождение
от старой копии того же самого. Сохраняйте оба файла `money-bonus-*.bin` при
переносе SD-карты. При ошибке записи деньги не начисляются.
