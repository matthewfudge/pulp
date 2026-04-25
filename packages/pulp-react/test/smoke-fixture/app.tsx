// Smoke test fixture: render a tiny tree via @pulp/react and have
// pulp-screenshot dump a PNG. Used by tools/scripts/test-pulp-react-smoke.sh.

import { render, View, Row, Label, Button } from '../../src/index.js';

function App() {
  // Explicit width/height on the outer View matches the window dims
  // pulp-screenshot was launched with (--width 800 --height 600).
  // Pulp's Yoga layout rules require explicit container sizing —
  // flex-grow alone won't propagate down from the implicit root.
  return (
    <View width={800} height={600} background="#0a0e14">
      <Row gap={10} paddingLeft={20} paddingRight={20} alignItems="center" height={44} width={800}>
        <Label textColor="#ffffff">@pulp/react smoke test</Label>
      </Row>
      <View flexGrow={1} background="#070a0e" alignItems="center" justifyContent="center" width={800}>
        <Label textColor="#a3a8b5">Hello from the native render path</Label>
      </View>
      <Row gap={6} paddingLeft={20} paddingRight={20} alignItems="center" height={44} width={800} background="#0a0e14">
        <Button>OK</Button>
        <Button>CANCEL</Button>
      </Row>
    </View>
  );
}

render(<App />);
