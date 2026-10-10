using Godot;

namespace R1AotSample;

// Minimal script type used to exercise the NativeAOT publish path.
// It mirrors the shape of the R-1 probe (exported member + signal) so that
// source generators and GodotSharp reflection metadata are part of the publish.
public partial class Main : Node
{
    [Export]
    public int Health { get; set; } = 10;

    [Signal]
    public delegate void PingedEventHandler(int value);

    public override void _Ready()
    {
        GD.Print("R1AOT_SAMPLE ready");
        EmitSignal(SignalName.Pinged, 7);
    }
}
