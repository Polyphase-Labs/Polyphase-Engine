using Polyphase;

public class Rotator : Script3D
{
    [Property] public Vector3 AngularVelocity = new Vector3(0, 90, 0);
    [Property(Display = "Spin Enabled")] public bool Enabled = true;

    private float mElapsed;

    public override void Start()
    {
        Log.Debug("Rotator started on " + Name);
    }

    public override void Tick(float deltaTime)
    {
        if (!Enabled) { return; }
        mElapsed += deltaTime;
        AddRotation(AngularVelocity * deltaTime);
    }
}
