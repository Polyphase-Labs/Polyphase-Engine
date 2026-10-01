using Polyphase;

// Exercises: ScriptWidget bare calls, widget creation via Node.Construct, the
// generated static API (TimerManager / Engine), callback delegates (timer +
// signal), Vector2 properties, [Replicated] + OnRep, [NetFunc] + InvokeNetFunc.
public class Hud : ScriptWidget
{
    [Property] public Vector2 offset = new Vector2(10, 20);
    [Replicated(OnRep = nameof(OnScoreChanged))] public int score = 0;
    [Property] [Replicated] public string title = "HUD";

    public int repCount;
    public int ticks;
    public int lastPoints;
    public Text label;

    private int mTimerId;

    public override void Start()
    {
        label = CreateNode("Text") as Text;
        label.SetAnchorMode(AnchorMode.TopLeft);
        label.SetPosition(offset.X, offset.Y);
        label.SetText(title);
        label.Attach(Node);

        mTimerId = TimerManager.SetTimer(() => ticks++, 0.5f, true);

        Node.ConnectSignal("Scored", Node, (listener, points) =>
        {
            lastPoints = (int)points;
            score += lastPoints;
        });
    }

    public void OnScoreChanged()
    {
        repCount++;
    }

    [NetFunc(NetFuncType.Server, Reliable = true)]
    public void S_AddScore(int points)
    {
        score += points;
    }

    [NetFunc(NetFuncType.Multicast)]
    public void M_Flash()
    {
    }

    public void RequestScore(int points)
    {
        InvokeNetFunc("S_AddScore", points);
    }

    public override void Tick(float deltaTime)
    {
        SetOpacityFloat(1.0f);     // bare Widget call through ScriptWidget
    }

    public float Elapsed()
    {
        return Engine.GetTime();
    }

    public string Kind()
    {
        return Node is Widget ? "widget" : "node";
    }

    public override void Stop()
    {
        TimerManager.ClearTimer(mTimerId);
    }
}
