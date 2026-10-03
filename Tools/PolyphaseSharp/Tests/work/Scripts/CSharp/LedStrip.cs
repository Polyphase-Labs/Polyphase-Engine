using Polyphase;

// Exercises engine handle types (Spline3D, StaticMesh3D, Material/MaterialLite),
// `as` / `is` on engine values, and [Property] arrays.
public class LedStrip : Script3D
{
    [Property] public int count = 160;
    [Property] public Spline3D spline;
    [Property] public StaticMesh3D[] leds;
    [Property] public float[] weights = new float[] { 1, 2, 3 };
    [Property] public string[] labels = new string[2];
    [Property] public Color tint = new Color(1, 0.5f, 0);

    private int mColored;

    public void SetLEDs(StaticMesh3D[] newLEDs)
    {
        leds = newLEDs;
    }

    public void SetSpline(Spline3D newSpline)
    {
        spline = newSpline;
    }

    public bool SetLEDColor(int index, Color color)
    {
        if (index < 0 || index >= leds.Length)
            return false;

        var meshInstance = leds[index];
        if (meshInstance == null)
            return false;

        var material = meshInstance.GetMaterial() as MaterialLite;
        if (material == null)
            return false;

        material.SetColor(color);
        material.SetBlendMode(BlendMode.Additive);
        material.ShadingModel = ShadingModel.Unlit;
        mColored++;
        return true;
    }

    public int GetColoredCount()
    {
        return mColored;
    }

    public float SumWeights()
    {
        float sum = 0;
        foreach (float w in weights)
            sum += w;
        return sum;
    }

    public int CountMeshes()
    {
        int n = 0;
        for (int i = 0; i < leds.Length; ++i)
        {
            if (leds[i] != null && leds[i] is Mesh3D)
                ++n;
        }
        return n;
    }

    public float SplineLength()
    {
        return spline != null ? spline.GetSplineLength() : -1;
    }

    public Vector3 PointAt(int index)
    {
        return spline.GetSplinePointWorldPosition(index);
    }

    public bool SelfIsStaticMesh()
    {
        return Node is StaticMesh3D;
    }

    public string MaterialKind(int index)
    {
        Material m = leds[index].GetMaterial();
        if (m is MaterialLite) return "lite";
        if (m is MaterialInstance) return "instance";
        return m.GetTypeName();
    }

    public override void Tick(float deltaTime)
    {
    }
}
