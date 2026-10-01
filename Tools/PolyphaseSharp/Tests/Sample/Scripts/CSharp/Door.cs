using Polyphase;

// Exercises the generic Script<TNode> base: Node is typed without casts.
public class Door : Script<StaticMesh3D>
{
    [Property] public float openAngle = 90;

    public string MaterialKind()
    {
        Material m = Node.GetMaterial();
        return m is MaterialLite ? "lite" : m.GetTypeName();
    }

    public bool HasTriangleCollision()
    {
        return Node.UseTriangleCollision;
    }
}
