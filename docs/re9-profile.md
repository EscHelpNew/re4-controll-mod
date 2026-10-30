# RE9 profile

Select **Controls > Profiles and reset > Controls profile > RE9** in either
interface. Use RE4's default native controller layout. Reapply RE9 after updating
from0.8 to activate the revised preset; saved Custom controls are preserved.
Graphics has separate profiles. Changing a control from RE9 creates Custom.

The preset enables **Unified sticks**: the left stick moves without steering,
and the right stick turns and looks during walking and supported firearm aiming.
**Camera > Right-stick sensitivity** is shared by these modes, together with the
right-stick deadzone and response curve. Separate classic turn/aim gains do not
apply in this mode. Default and old saved settings keep Unified sticks off.

The preset also enables moving aim/knife, firearm-only gyro at0.80, free360 look
and a small half-transparent crosshair. Third-person distance starts at1.35.
**Camera > Third-person distance** ranges from1.00 (original framing) to2.00.
It moves the camera before native wall collision; narrow spaces may pull it in.
First person, optical sights and cinematic cameras retain their own ownership.

| Physical input | Action in this profile |
|---|---|
| Left stick | Move, including sideways movement in supported gameplay states |
| Right stick | Turn / look / aim with common sensitivity |
| Hold L3 | Run through the original B-button action |
| A / B / X / Y | Original RE4 actions |
| PLUS / MINUS | Original RE4 actions |
| D-pad | Original mapping |
| ZL / ZR | Ready gun / fire, using RE4's default layout |
| L + ZR | Ready knife + slash |
| R | Original RE4 action |

L3 remains an alias of the native B action, so its other contextual actions can
also apply. This profile does not change native button prompts or add RE9
crouching, parrying, quick healing, quick weapon slots or modern animations.

For supported ordinary firearm views, the camera follows the fresh native
weapon-ray endpoint and the custom reticle is centered on that aim. Unsupported
weapons, scopes and special camera states retain native behavior. The shared
input formula does not claim identical animation response for every weapon.
Movement, transitions, walls and aiming alignment still need hardware acceptance
for each new build; host tests do not reproduce the whole game engine.

For a console check, reapply RE9 and compare a small/full right-stick deflection
while walking and aiming at a common sensitivity of0.30. With the right stick
released, test all four left-stick directions. Check walking-to-aim pitch,
release/re-aim and post-shot recovery. Compare the centered translucent marker
with the laser/hit endpoint near walls, and verify original face/menu buttons.

The initial layout research visually checked the
[RE9 in-game PS5 layout screenshot](https://www.spottis.com/img/public/resident-evil-requiem-controls-playstation.jpg),
[RE9 Switch diagram](https://img.gamewith.jp/img/ec96ddc7bf46f4b390c10f9f731e09a2.png)
and [Capcom's original RE4 Switch controls](https://game.capcom.com/manual/bio4/img/manual/en/switch/5.jpg).
The0.9 preset preserves original face/menu buttons at the user's request. It is
an adaptation to this port, not a claim to reproduce undocumented RE9 formulas.
